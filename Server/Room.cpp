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

Room::Room(Net::MatchContext context, Target& targets,
	std::vector<TargetRecipient> recipients, PacketSender sender)
	: Room()
{
	m_state.version.context = context;
	targets_ = &targets;
	targetRecipients_ = std::move(recipients);
	m_fSendPacket = std::move(sender);
}

Room::~Room()
{
	DeleteCriticalSection(&commandCS_);
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
	//임계 영역에서 큐의 데이터만 뽑아온 뒤 원래대로 통신받을 수 있도록 하기 위함
	std::queue<Net::RoomCommand> commands;
	EnterCriticalSection(&commandCS_);
	commands.swap(commandQueue_);
	LeaveCriticalSection(&commandCS_);

	//현재 while문 안에는 플레이어 이동 처리만 담당중임.
	while (!commands.empty() && !stopRequested_)
	{
		//큐에서 요청 하나 읽은 뒤
		Net::RoomCommand& command = commands.front();
		//명령이 PlayerTransform이 맞을 때 Get
		Net::PlayerTransform* input = std::get_if<Net::PlayerTransform>(&command.body);
		//방 ID와 매치 ID를 가져온 뒤
		Net::MatchContext& context = m_state.version.context;
		//명령 가져오기 성공 && 방과 매칭 ID 정보가 같을 때 플레이어의 이동 요청을 처리함
		if (input && command.context.roomId == context.roomId && command.context.matchId == context.matchId)
			UpdatePlayerTransform(command.sessionToken, *input, command.receivedAtMs);

		//읽은 요청 pop
		commands.pop();
	}
}

void Room::Tick()
{
	ProcessCommands();
	SendTransformData();
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

void Room::SendTransformData()
{
	const Net::MatchContext& context = m_state.version.context;
	if (stopRequested_ || m_state.state != Net::GameState::Playing ||
		!m_fSendPacket || targetRecipients_.empty() || context.roomId == 0 || context.matchId == 0 ||
		m_vPlayers.size() > Net::MaxPlayers)
		return;

	//스냅샷(순간적인 방의 상태)을 생성한 서버시각
	Net::ServerTimeMs nowMs = GetTickCount64();
	Net::UdpPacket<Net::PlayerSnapshot> snapshot;

	//data는 패킷의 실제 데이터 부분을 레퍼런스로 갖는 변수임.
	Net::PlayerSnapshot& data = snapshot.packetData;
	data.serverTimeMs = nowMs;
	//리비전 = 방 상태 갱신 순번
	//클라 입장에서 패킷 데이터를 받아 변환한 뒤, 리비전을 먼저 체크해서
	//클라가 기록한 마지막 방 상태 순번보다 이전 순번인 경우 => 이후 처리는 하지 않음
	data.requiredRevision = m_state.version.revision;
	data.playerCount = static_cast<std::uint8_t>(m_vPlayers.size());

	//서버 이동·충돌 처리가 끝난 실제 플레이어 데이터를 기록
	for (size_t index = 0; index < m_vPlayers.size(); ++index)
	{
		//토큰
		data.players[index].m_sessionToken = m_vPlayers[index].m_PlayerData.m_sessionToken;
		//현재 변환 상태
		data.players[index].pose = m_vPlayers[index].m_PlayerData.pose;
		//마지막으로 처리한 이동 요청 순번
		data.players[index].lastProcessedInput = m_vPlayers[index].m_nLastProcessedInput;
	}

	//헤더 설정
	snapshot.header.type = Net::UdpType::PlayerSnapshot;
	snapshot.header.context = context;
	snapshot.header.sequence = ++m_nSnapshotSequence;
	snapshot.header.serverTick = static_cast<std::uint32_t>(nowMs * Net::ServerTickRate / 1000);

	// 본문은 동일하고, 헤더의 수신자 토큰과 재입장 식별자만 각 접속자에 맞춘다.
	for (const TargetRecipient& recipient : targetRecipients_)
	{
		snapshot.header.m_sessionToken = recipient.sessionToken;
		snapshot.header.membershipId = recipient.membershipId;
		Net::SendRequest request{recipient.sessionToken, {}};

		//데이터 네트워크 바이트로 변환하여 저장
		if (Net::PacketCodec::EncodeUdp(snapshot, request.packet))
			//전송
			m_fSendPacket(request);
	}
}

void Room::SendTargetPositions(Net::ServerTimeMs nowMs)
{
	const Net::MatchContext& context = m_state.version.context;
	if (!m_fSendPacket || targetRecipients_.empty() || context.roomId == 0 || context.matchId == 0)
		return;

	std::vector<Net::TargetMotionData> activeTargets = targets_->GetActiveTargets(nowMs);
	Net::UdpPacket<Net::TargetSnapshot> snapshot;
	snapshot.packetData.targetCount = static_cast<std::uint16_t>(activeTargets.size());
	std::copy(activeTargets.begin(), activeTargets.end(), snapshot.packetData.targets.begin());

	Net::UdpHeader& header = snapshot.header;
	header.type = Net::UdpType::TargetSnapshot;
	header.context = context;
	header.sequence = ++m_nSnapshotSequence;
	header.serverTick = static_cast<std::uint32_t>(nowMs * Net::ServerTickRate / 1000);

	for (const TargetRecipient& recipient : targetRecipients_)
	{
		header.m_sessionToken = recipient.sessionToken;
		header.membershipId = recipient.membershipId;
		Net::SendRequest request{recipient.sessionToken, {}};
		if (Net::PacketCodec::EncodeUdp(snapshot, request.packet))
			m_fSendPacket(request);
	}
}
