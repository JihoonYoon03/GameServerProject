#include "pch.h"
#include "Room.h"
#include "../Shared/PacketCodec.h"

#include <algorithm>

Room::Room()
{
	InitializeCriticalSection(&commandCS_);
}

Room::Room(Net::MatchContext context, Target& targets,
	std::vector<TargetRecipient> recipients, PacketSender sender)
	: Room()
{
	m_state.version.context = context;
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

void Room::SendTargetPositions(Net::ServerTimeMs nowMs)
{
	const auto& context = m_state.version.context;
	if (!sendPacket_ || targetRecipients_.empty() || context.roomId == 0 || context.matchId == 0)
		return;

	const auto activeTargets = targets_->GetActiveTargets(nowMs);
	Net::UdpPacket<Net::TargetSnapshot> snapshot;
	snapshot.packetData.targetCount = static_cast<std::uint16_t>(activeTargets.size());
	std::copy(activeTargets.begin(), activeTargets.end(), snapshot.packetData.targets.begin());

	auto& header = snapshot.header;
	header.type = Net::UdpType::TargetSnapshot;
	header.context = context;
	header.sequence = ++targetSnapshotSequence_;
	header.serverTick = static_cast<std::uint32_t>(nowMs * Net::ServerTickRate / 1000);
	header.size = static_cast<std::uint16_t>(Net::TargetSnapshotFixedBytes +
		snapshot.packetData.targetCount * Net::TargetMotionBytes);

	for (const auto& recipient : targetRecipients_)
	{
		header.m_sessionToken = recipient.sessionToken;
		header.membershipId = recipient.membershipId;
		Net::SendRequest request{recipient.sessionToken, {}};
		if (Net::PacketCodec::EncodeUdp(snapshot, request.packet))
			sendPacket_(request);
	}
}
