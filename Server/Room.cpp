#include "pch.h"
#include "Room.h"
#include "../Shared/PacketCodec.h"

#include <algorithm>
#include <utility>
#include <variant>

Room::Room()
{
	InitializeCriticalSection(&commandCS_);
}

Room::Room(Net::RoomId roomId, PacketSender sender,
	std::queue<Net::RoomManagementEvent>& managementQueue, CRITICAL_SECTION& managementCS)
	: Room()
{
	m_state.version.context.roomId = roomId;
	m_state.capacity = 1;
	sendPacket_ = std::move(sender);
	managementQueue_ = &managementQueue;
	managementCS_ = &managementCS;
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
	DeleteCriticalSection(&commandCS_);
}

void Room::AssignPlayerAreas()
{
	if (m_state.vAreas.empty())
	{
		// 기존 사격장의 남쪽 구역과 시작 위치를 그대로 사용한다.
		Net::AreaData area;
		area.areaId = 0;
		area.movementBounds.min.x = -2.0f;
		area.movementBounds.min.z = 7.0f;
		area.movementBounds.max.x = 2.0f;
		area.movementBounds.max.z = 11.0f;
		area.spawnTransform[3] = glm::vec4(0.0f, 1.0f, 9.0f, 1.0f);
		m_state.vAreas.push_back(area);
	}
	for (ClientPlayer& player : m_vPlayers)
	{
		player.m_PlayerData.areaId = m_state.vAreas[0].areaId;
		player.m_PlayerData.pose.position = glm::vec3(m_state.vAreas[0].spawnTransform[3]);
	}
}

void Room::EnterPlayer(const Net::RoomCommand& command, const Net::RoomEnterCommand& enter)
{
	Net::RoomEnterResult result;
	if (!enter.UDPRegistered)
		result.result = Net::ErrorCode::UdpNotRegistered;
	else if (!Net::IsValidSessionToken(command.sessionToken) || command.membershipId == 0)
		result.result = Net::ErrorCode::InvalidSession;
	else if (!m_vPlayers.empty())
		result.result = Net::ErrorCode::RoomFull;
	else
	{
		ClientPlayer player;
		player.m_PlayerData.m_sessionToken = command.sessionToken;
		m_vPlayers.push_back(player);
		targetRecipients_.push_back({command.sessionToken, command.membershipId});
		AssignPlayerAreas();
		++m_state.version.revision;
		m_state.vPlayers = {m_vPlayers[0].m_PlayerData};
		result.membershipId = command.membershipId;
		result.room = m_state;
		PublishManagementEvent(Net::RoomManagementEventType::SummaryChanged);
	}
	Net::SendRequest response;
	response.recipientToken = command.sessionToken;
	if (sendPacket_ && Net::PacketCodec::EncodeTcp(result, command.requestId, response.packet))
		sendPacket_(response);
	if (m_vPlayers.empty())
	{
		PublishManagementEvent(Net::RoomManagementEventType::RoomEmpty);
		RequestStop();
	}
}

void Room::PublishManagementEvent(Net::RoomManagementEventType type)
{
	if (managementQueue_ == nullptr || managementCS_ == nullptr)
		return;
	Net::RoomManagementEvent event;
	event.type = type;
	event.summary.roomId = m_state.version.context.roomId;
	event.summary.playerCount = static_cast<std::uint8_t>(m_vPlayers.size());
	event.summary.capacity = m_state.capacity;
	event.summary.state = m_state.state;
	EnterCriticalSection(managementCS_);
	try
	{
		managementQueue_->push(event);
	}
	catch (...)
	{
		LeaveCriticalSection(managementCS_);
		throw;
	}
	LeaveCriticalSection(managementCS_);
}

void Room::RemovePlayerFromRoom(const Net::SessionToken& token)
{
	for (auto player = m_vPlayers.begin(); player != m_vPlayers.end(); ++player)
	{
		if (player->m_PlayerData.m_sessionToken != token)
			continue;
		m_vPlayers.erase(player);
		for (auto recipient = targetRecipients_.begin(); recipient != targetRecipients_.end(); ++recipient)
		{
			if (recipient->sessionToken == token)
			{
				targetRecipients_.erase(recipient);
				break;
			}
		}
		++m_state.version.revision;
		if (m_vPlayers.empty())
		{
			m_state.vPlayers.clear();
			PublishManagementEvent(Net::RoomManagementEventType::RoomEmpty);
			RequestStop();
		}
		else
			SendRoomState();
		return;
	}
}

void Room::SetPlayerLoaded(const Net::SessionToken& token)
{
	if (m_state.state != Net::GameState::Preparing)
		return;

	for (ClientPlayer& player : m_vPlayers)
	{
		Net::PlayerData& data = player.m_PlayerData;
		if (data.m_sessionToken != token)
			continue;
		if (data.mapLoaded != 0)
			return;

		data.mapLoaded = 1;
		data.ready = Net::ReadyState::Waiting;
		++m_state.version.revision;
		SendRoomState();
		StartGameIfReady();
		return;
	}
}

void Room::StartGameIfReady()
{
	// 1인 전용 방은 로딩이 끝나면 별도 준비 버튼 없이 시작한다.
	if (m_state.capacity != 1 || m_state.state != Net::GameState::Preparing
		|| m_vPlayers.size() != 1 || m_vPlayers[0].m_PlayerData.mapLoaded == 0)
		return;

	Net::ServerTimeMs nowMs = GetTickCount64();
	// 이성노 후속 연결: 경기 관리자의 시작 시각을 공지와 공유할 수 있는 인터페이스가 필요하다.
	stageController_.StartGame(m_state.rules, *targets_);
	++m_state.version.context.matchId;
	++m_state.version.revision;
	m_state.state = Net::GameState::Playing;
	m_vPlayers[0].m_PlayerData.ready = Net::ReadyState::Playing;
	m_vPlayers[0].m_PlayerData.stats = {};
	m_state.vPlayers = {m_vPlayers[0].m_PlayerData};

	Net::GameStarted event;
	event.version = m_state.version;
	event.serverTimeMs = nowMs;
	event.startsAtMs = nowMs;
	event.endsAtMs = nowMs + m_state.rules.durationMs;
	event.rules = m_state.rules;
	event.players = m_state.vPlayers;
	Net::SendRequest response;
	response.recipientToken = m_vPlayers[0].m_PlayerData.m_sessionToken;
	if (sendPacket_ && Net::PacketCodec::EncodeTcp(event, 0, response.packet))
		sendPacket_(response);
	PublishManagementEvent(Net::RoomManagementEventType::SummaryChanged);
}

void Room::SendRoomState()
{
	// 참가자 정보는 방이 소유한 현재 상태에서 복사한다.
	m_state.vPlayers.clear();
	for (const ClientPlayer& player : m_vPlayers)
		m_state.vPlayers.push_back(player.m_PlayerData);
	PublishManagementEvent(Net::RoomManagementEventType::SummaryChanged);

	if (!sendPacket_)
		return;

	Net::RoomState state;
	state.room = m_state;
	Net::PacketData packet;
	if (!Net::PacketCodec::EncodeTcp(state, 0, packet))
		return;

	for (const ClientPlayer& player : m_vPlayers)
	{
		Net::SendRequest request;
		request.recipientToken = player.m_PlayerData.m_sessionToken;
		request.packet = packet;
		sendPacket_(request);
	}
}

DWORD WINAPI Room::RoomProc(LPVOID parameter)
{
	Room* room = static_cast<Room*>(parameter);
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
			room->RequestStop();
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
	return result;
}

void Room::RequestStop()
{
	stopRequested_ = true;
}

bool Room::AddCommand(const Net::RoomCommand& command)
{
	EnterCriticalSection(&commandCS_);
	if (stopRequested_)
	{
		LeaveCriticalSection(&commandCS_);
		return false;
	}
	try
	{
		commandQueue_.push(command);
	}
	catch (...)
	{
		LeaveCriticalSection(&commandCS_);
		throw;
	}
	LeaveCriticalSection(&commandCS_);
	return true;
}

void Room::ProcessCommands()
{
	// 명령만 옮긴 뒤 잠금을 풀어 서버 메인이 다음 명령을 넣을 수 있게 한다.
	std::queue<Net::RoomCommand> commands;
	EnterCriticalSection(&commandCS_);
	commands.swap(commandQueue_);
	LeaveCriticalSection(&commandCS_);

	while (!commands.empty() && !stopRequested_)
	{
		Net::RoomCommand command = commands.front();
		commands.pop();
		ProcessCommand(command);
	}
}

void Room::ProcessCommand(const Net::RoomCommand& command)
{
	const Net::MatchContext& context = m_state.version.context;
	if (command.context.roomId != context.roomId)
		return;
	const Net::RoomEnterCommand* enter = std::get_if<Net::RoomEnterCommand>(&command.body);
	if (enter)
	{
		if (command.context.matchId == context.matchId)
			EnterPlayer(command, *enter);
		return;
	}

	bool member = false;
	for (const TargetRecipient& recipient : targetRecipients_)
	{
		if (recipient.sessionToken == command.sessionToken && recipient.membershipId == command.membershipId)
		{
			member = true;
			break;
		}
	}
	if (!member)
		return;

	// 종료는 경기 시작 공지가 송신되기 전의 경기 번호로도 도착할 수 있다.
	if (std::get_if<Net::ClientDisconnected>(&command.body))
	{
		RemovePlayerFromRoom(command.sessionToken);
		return;
	}
	const Net::LeaveRoomRequest* leave = std::get_if<Net::LeaveRoomRequest>(&command.body);
	if (leave)
	{
		if (leave->roomId != context.roomId)
			return;
		Net::LeaveRoomResult result;
		result.roomId = context.roomId;
		Net::SendRequest response;
		response.recipientToken = command.sessionToken;
		if (sendPacket_ && Net::PacketCodec::EncodeTcp(result, command.requestId, response.packet))
			sendPacket_(response);
		RemovePlayerFromRoom(command.sessionToken);
		return;
	}
	if (command.context.matchId != context.matchId)
		return;
	const Net::MapLoaded* loaded = std::get_if<Net::MapLoaded>(&command.body);
	if (loaded)
	{
		if (loaded->roomId == context.roomId)
			SetPlayerLoaded(command.sessionToken);
		return;
	}
	// 윤지훈 후속 연결: 이동 결과 송신과 FireRequest 사격 처리를 연결한다.
	const Net::PlayerTransform* input = std::get_if<Net::PlayerTransform>(&command.body);
	if (input)
		UpdatePlayerTransform(command.sessionToken, *input, command.receivedAtMs);
}

void Room::Tick()
{
	ProcessCommands();
	// 이성노 후속 연결: 경기 틱 갱신과 SendTargetPositions 호출을 연결한다.
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
		return entry.m_PlayerData.m_sessionToken == token;
	});
	if (player == m_vPlayers.end())
		return false;

	//찾은 플레이어가 배정받은 구역과 일치하는 구역 탐색(이동 구역 경계 제한을 위해)
	const auto area = std::find_if(m_state.vAreas.begin(), m_state.vAreas.end(), [&](const Net::AreaData& entry)
	{
		return entry.areaId == player->m_PlayerData.areaId;
	});

	//구역 탐색 성공한 경우 이동 계산 후 리턴
	return area != m_state.vAreas.end() && player->ApplyMovement(input, *area, m_vWorldBoxes, receivedAtMs);
}

void Room::SendTargetPositions(Net::ServerTimeMs nowMs)
{
	const Net::MatchContext& context = m_state.version.context;
	if (!sendPacket_ || targetRecipients_.empty() || context.roomId == 0 || context.matchId == 0)
		return;

	std::vector<Net::TargetMotionData> activeTargets = targets_->GetActiveTargets(nowMs);
	Net::UdpPacket<Net::TargetSnapshot> snapshot;
	snapshot.packetData.targetCount = static_cast<std::uint16_t>(activeTargets.size());
	std::copy(activeTargets.begin(), activeTargets.end(), snapshot.packetData.targets.begin());

	Net::UdpHeader& header = snapshot.header;
	header.type = Net::UdpType::TargetSnapshot;
	header.context = context;
	header.sequence = ++targetSnapshotSequence_;
	header.serverTick = static_cast<std::uint32_t>(nowMs * Net::ServerTickRate / 1000);

	for (const TargetRecipient& recipient : targetRecipients_)
	{
		header.m_sessionToken = recipient.sessionToken;
		header.membershipId = recipient.membershipId;
		Net::SendRequest request{recipient.sessionToken, {}};
		if (Net::PacketCodec::EncodeUdp(snapshot, request.packet))
			sendPacket_(request);
	}
}
