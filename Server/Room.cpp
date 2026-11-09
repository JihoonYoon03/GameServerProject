#include "pch.h"
#include "Room.h"
#include "Client.h"

#include <algorithm>
#include <cstring>
#include <limits>

Room::Room()
{
	InitializeCriticalSection(&commandCS_);
}

Room::Room(Net::MatchContext context, Target& targets,
	std::vector<TargetRecipient> recipients, PacketSender sender)
	: Room()
{
	m_state.version.context = context;
	targetContext_ = context;
	targets_ = &targets;
	targetRecipients_ = std::move(recipients);
	sendPacket_ = std::move(sender);
}

Room::~Room()
{
	StopRoom();
	DeleteCriticalSection(&commandCS_);
}

void Room::SetHandlers(CommandHandler commandHandler, TickHandler tickHandler)
{
	if (roomThread_ != nullptr)
		return;

	commandHandler_ = std::move(commandHandler);
	tickHandler_ = std::move(tickHandler);
}

void Room::StartRoomThread()
{
	if (roomThread_ != nullptr)
		return;

	stopRequested_ = false;
	roomThread_ = CreateThread(nullptr, 0, RoomProc, this, 0, nullptr);
	if (roomThread_ == nullptr)
	{
		stopRequested_ = true;
		err_display(static_cast<int>(GetLastError()));
	}
}

DWORD WINAPI Room::RoomProc(LPVOID parameter)
{
	Room* room = static_cast<Room*>(parameter);
	room->roomThreadId_ = GetCurrentThreadId();
	DWORD result = 0;
	const DWORD intervalMs = 1000 / Net::ServerTickRate;
	const DWORD remainderMs = 1000 % Net::ServerTickRate;
	DWORD accumulatedRemainder = 0;
	ULONGLONG nextTick = GetTickCount64();

	while (!room->stopRequested_)
	{
		try
		{
			room->Tick();
		}
		catch (...)
		{
			room->stopRequested_ = true;
			fputs("방 갱신 중 예외가 발생하여 방 스레드를 종료합니다.\n", stderr);
			result = 1;
			break;
		}

		// 나눗셈의 나머지를 쌓아 초당 갱신 횟수가 어긋나지 않게 한다.
		nextTick += intervalMs;
		accumulatedRemainder += remainderMs;
		if (accumulatedRemainder >= Net::ServerTickRate)
		{
			++nextTick;
			accumulatedRemainder -= Net::ServerTickRate;
		}

		ULONGLONG now = GetTickCount64();
		if (now < nextTick && !room->stopRequested_)
			Sleep(static_cast<DWORD>(nextTick - now));
		else if (now > nextTick + intervalMs)
			nextTick = now; // 늦어진 갱신을 한꺼번에 반복하지 않는다.
	}
	room->roomThreadId_ = 0;
	return result;
}

void Room::AddCommand(const Net::RoomCommand& command)
{
	EnterCriticalSection(&commandCS_);
	try
	{
		if (!stopRequested_)
			commandQueue_.push(command);
	}
	catch (...)
	{
		LeaveCriticalSection(&commandCS_);
		throw;
	}
	LeaveCriticalSection(&commandCS_);
}

void Room::ProcessCommands()
{
	// 콜백이 새 명령을 넣을 수 있도록 큐를 옮긴 뒤 잠금을 푼다.
	std::queue<Net::RoomCommand> commands;
	EnterCriticalSection(&commandCS_);
	commands.swap(commandQueue_);
	LeaveCriticalSection(&commandCS_);

	while (!commands.empty() && !stopRequested_)
	{
		if (commandHandler_)
			commandHandler_(commands.front());
		commands.pop();
	}
}

void Room::Tick()
{
	ProcessCommands();
	if (!stopRequested_ && tickHandler_)
		tickHandler_();
}

void Room::StopRoom()
{
	stopRequested_ = true;
	// 방 스레드는 자기 종료를 기다리지 않는다. 핸들은 관리 스레드가 정리한다.
	if (roomThreadId_ == GetCurrentThreadId())
		return;

	if (roomThread_ != nullptr)
	{
		WaitForSingleObject(roomThread_, INFINITE);
		CloseHandle(roomThread_);
		roomThread_ = nullptr;
	}

	// 방 스레드가 종료된 뒤 아직 처리하지 않은 명령을 비운다.
	EnterCriticalSection(&commandCS_);
	while (!commandQueue_.empty())
		commandQueue_.pop();
	LeaveCriticalSection(&commandCS_);
}

void Room::RecvPlayerInput(const Net::SessionToken& player,
	const Net::PlayerTransform& input, Net::ServerTimeMs receivedAtMs)
{
	UpdatePlayerTransform(player, input, receivedAtMs);
}

bool Room::UpdatePlayerTransform(const Net::SessionToken& token,
	const Net::PlayerTransform& input, Net::ServerTimeMs receivedAtMs)
{
	//방이 플레이 중인가
	if (m_state.state != Net::GameState::Playing)
		return false;

	//토큰과 일치하는 플레이어 찾기
	const auto player = std::find_if(m_vPlayers.begin(), m_vPlayers.end(), [&](const ClientPlayer& entry)
	{
		return entry.state.m_sessionToken == token;
	});
	if (player == m_vPlayers.end())
		return false;

	//찾은 플레이어가 배정받은 구역과 일치하는 구역 탐색(이동 구역 경계 제한을 위해)
	const auto area = std::find_if(m_state.vAreas.begin(), m_state.vAreas.end(), [&](const Net::AreaData& entry)
	{
		return entry.areaId == player->state.areaId;
	});

	//구역 탐색 성공한 경우 이동 계산 후 리턴
	return area != m_state.vAreas.end() && player->ApplyMovement(input, *area, m_vWorldBoxes, receivedAtMs);
}

void Room::SendTargetPositions(SOCKET udpSocket, const std::vector<Client>& clients,
	Net::ServerTimeMs nowMs)
{
	if (!targets_ || udpSocket == INVALID_SOCKET || clients.empty() || targetContext_.roomId == 0) return;

	const std::vector<Net::TargetMotionData> activeTargets = targets_->GetActiveTargets(nowMs);
	if (activeTargets.size() > Net::MaxTargets) return;

	const auto totalTargetCount = static_cast<std::uint16_t>(activeTargets.size());
	const auto chunkCount = static_cast<std::uint16_t>((std::max)(std::size_t{1},
		(activeTargets.size() + Net::MaxTargetsPerSnapshot - 1) / Net::MaxTargetsPerSnapshot));
	const Net::Sequence sequence = ++targetSnapshotSequence_;
	const auto serverTick = static_cast<std::uint32_t>(nowMs * Net::ServerTickRate / 1000);

	// 현재 UDP 헤더에는 버전 필드가 없으므로 선언된 필드만 직렬화한다.
	constexpr std::uint16_t HeaderBytes = 2 + 2 + 2 + Net::SessionTokenBytes + 4 + 4 + 4 + 4 + 4;
	constexpr std::uint16_t SnapshotMetadataBytes = 8 + 2 + 2 + 2 + 2;
	constexpr std::uint16_t TargetBytes = 4 + 3 * 4 + 3 * 4;
	static_assert(HeaderBytes + SnapshotMetadataBytes +
		Net::MaxTargetsPerSnapshot * TargetBytes <= Net::MaxUdpPacketBytes);

	for (std::uint16_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
	{
		const std::size_t offset = chunkIndex * Net::MaxTargetsPerSnapshot;
		const auto targetCount = static_cast<std::uint16_t>((std::min)(
			std::size_t{Net::MaxTargetsPerSnapshot}, activeTargets.size() - offset));
		const auto packetSize = static_cast<std::uint16_t>(
			HeaderBytes + SnapshotMetadataBytes + targetCount * TargetBytes);

		for (const auto& client : clients)
		{
			if (!client.IsUDPRegistered() || client.GetRoomId() != targetContext_.roomId) continue;

			std::vector<std::uint8_t> bytes;
			bytes.reserve(packetSize);

			WriteUInt16(bytes, Net::UdpMagic);
			WriteUInt16(bytes, static_cast<std::uint16_t>(Net::UdpType::TargetSnapshot));
			WriteUInt16(bytes, packetSize);
			bytes.insert(bytes.end(), client.GetSessionToken().begin(), client.GetSessionToken().end());
			WriteUInt32(bytes, sequence);
			WriteUInt32(bytes, targetContext_.roomId);
			WriteUInt32(bytes, targetContext_.matchId);
			WriteUInt32(bytes, client.GetMembershipId());
			WriteUInt32(bytes, serverTick);

			WriteUInt64(bytes, nowMs);
			WriteUInt16(bytes, totalTargetCount);
			WriteUInt16(bytes, chunkIndex);
			WriteUInt16(bytes, chunkCount);
			WriteUInt16(bytes, targetCount);
			for (std::size_t index = offset; index < offset + targetCount; ++index)
			{
				const auto& target = activeTargets[index];
				WriteUInt32(bytes, target.targetId);
				WriteFloat(bytes, target.position.x);
				WriteFloat(bytes, target.position.y);
				WriteFloat(bytes, target.position.z);
				WriteFloat(bytes, target.velocity.x);
				WriteFloat(bytes, target.velocity.y);
				WriteFloat(bytes, target.velocity.z);
			}

			// 이 방에 속하고 UDP 등록을 마친 참가자의 주소로 전송한다.
			const int result = sendto(udpSocket,
				reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()),
				0, reinterpret_cast<const sockaddr*>(&client.GetUDPAddress()), sizeof(client.GetUDPAddress()));
			if (result == SOCKET_ERROR) err_display("sendto()");
		}
	}
}

// 원본 값은 유지하고 전송 버퍼에 기록할 때 네트워크 바이트 순서로 변환한다.
void Room::WriteUInt16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
{
	const std::uint16_t networkValue = htons(value);
	const auto* data = reinterpret_cast<const std::uint8_t*>(&networkValue);
	bytes.insert(bytes.end(), data, data + sizeof(networkValue));
}

void Room::WriteUInt32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
	const std::uint32_t networkValue = htonl(value);
	const auto* data = reinterpret_cast<const std::uint8_t*>(&networkValue);
	bytes.insert(bytes.end(), data, data + sizeof(networkValue));
}

void Room::WriteUInt64(std::vector<std::uint8_t>& bytes, std::uint64_t value)
{
	// 상위 32비트와 하위 32비트를 차례로 변환해 기록한다.
	WriteUInt32(bytes, static_cast<std::uint32_t>(value >> 32));
	WriteUInt32(bytes, static_cast<std::uint32_t>(value));
}

void Room::WriteFloat(std::vector<std::uint8_t>& bytes, float value)
{
	static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
	// 실수는 비트 표현을 복사한 뒤 정수 변환 함수를 적용한다.
	std::uint32_t bits;
	std::memcpy(&bits, &value, sizeof(bits));
	WriteUInt32(bytes, bits);
}
