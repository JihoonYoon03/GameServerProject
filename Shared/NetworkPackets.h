#pragma once
#include "NetworkTypes.h"

// 프로토콜 6: 공통 헤더 뒤에 본문 필드를 기록한다. 구조체의 메모리 크기를 전송 크기로 쓰지 않는다.
// 헤더의 길이 필드는 헤더를 포함한 전체 패킷 길이이다. 정수와 실수의 비트는
// 빅 엔디언으로 전송하고, 가변 목록은 개수와 유효 항목만 기록한다.
// UDP 토큰은 클라→서버에서 발신자, 서버→클라에서 수신자를 나타낸다.
namespace Net
{
	inline constexpr std::uint16_t ProtocolVersion = 6;
	inline constexpr std::uint16_t UdpMagic = 0x4347;
	inline constexpr std::uint32_t TcpHeaderBytes = 12;
	inline constexpr std::uint32_t MaxTcpPacketBytes = 65536;
	inline constexpr std::uint16_t UdpHeaderBytes = 44;
	inline constexpr std::uint16_t MaxUdpPacketBytes = 1200;
	inline constexpr std::uint16_t TargetSnapshotFixedBytes = UdpHeaderBytes + 2;
	inline constexpr std::uint16_t TargetMotionBytes = 24;
	static_assert(TargetSnapshotFixedBytes + MaxTargets * TargetMotionBytes <= MaxUdpPacketBytes);

	enum class TcpType : std::uint16_t
	{
		ConnectResult = 0x0001,
		RequestRejected = 0x0002,
		RoomListRequest = 0x0101,
		RoomListResult = 0x0102,
		CreateRoomRequest = 0x0110,
		JoinRoomRequest = 0x0111,
		RoomEnterResult = 0x0112,
		LeaveRoomRequest = 0x0113,
		LeaveRoomResult = 0x0114,
		MapLoaded = 0x0201,
		ReadyChangeRequest = 0x0202,
		RoomState = 0x0203,
		GameStarted = 0x0301,
		FireRequest = 0x0302,
		ShotResolved = 0x0303,
		MatchTime = 0x0305,
		GameResults = 0x0306,
	};

	enum class UdpType : std::uint16_t
	{
		RegisterRequest = 0x1001,
		RegisterResult = 0x1002,
		PlayerTransform = 0x1101,
		PlayerSnapshot = 0x1102,
		TargetSnapshot = 0x1103
	};

	// 사용: 서버 NetworkServer::NetworkLoop(), DispatchPacket() / 클라 NetworkClient::NetworkLoop(), UpdateNetwork() [선언]
	struct TcpHeader
	{
		std::uint32_t size = TcpHeaderBytes;
		std::uint16_t version = ProtocolVersion;
		TcpType type = TcpType::ConnectResult;
		std::uint32_t requestId = 0;
	};

	// 사용: 서버 NetworkServer::RegisterPlayerUDP(), DispatchPacket() / 클라 NetworkClient::NetworkLoop() [선언]
	struct UdpHeader
	{
		std::uint16_t magic = UdpMagic;
		std::uint16_t version = ProtocolVersion;
		UdpType type = UdpType::RegisterRequest;
		std::uint16_t size = UdpHeaderBytes;
		SessionToken m_sessionToken{};

		Sequence sequence = 0;
		MatchContext context;
		MembershipId membershipId = 0;
		std::uint32_t serverTick = 0;
	};

	// 사용: 서버 NetworkServer::AcceptPlayer() → 클라 NetworkClient::ConnectServer() [선언]
	struct ConnectResult { ErrorCode result = ErrorCode::Ok; SessionToken sessionToken{};

		std::uint16_t udpPort = 0;
		ServerTimeMs serverTimeMs = 0;
		std::uint16_t tickRate = static_cast<std::uint16_t>(ServerTickRate);
	};

	// 사용: 서버 NetworkServer::DispatchPacket(), RoomManager::JoinGameRoom(), Room::RecvShoot() → 클라
	//       NetworkClient::UpdateNetwork() [선언]
	struct RequestRejected { ErrorCode reason = ErrorCode::InvalidRequest; MatchContext context; };

	// 사용: 클라 MainScene::SearchRoomList() → 서버 RoomManager::SendRoomList() [선언]
	struct RoomListRequest {};

	// 사용: 서버 RoomManager::SendRoomList() → 클라 MainScene::UpdateRoomList() [선언]
	struct RoomListResult { std::vector<RoomInfo> rooms; };

	// 사용: 클라 MainScene::RequestGameRoom() → 서버 RoomManager::CreateGameRoom() [선언]
	struct CreateRoomRequest {};

	// 사용: 클라 MainScene::JoinRoom() → 서버 RoomManager::JoinGameRoom() [선언]
	struct JoinRoomRequest { RoomId roomId = 0; };

	// 사용: 서버 RoomManager::CreateGameRoom(), RoomManager::JoinGameRoom() → 클라 MainScene::JoinRoom() [선언]
	struct RoomEnterResult
	{
		ErrorCode result = ErrorCode::Ok;
		MembershipId membershipId = 0;
		RoomStateData room;
	};

	// 사용: 클라 MainScene::ExitRoom() → 서버 Room::RemovePlayerFromRoom() [선언]
	struct LeaveRoomRequest { RoomId roomId = 0; };

	// 사용: 서버 Room::RemovePlayerFromRoom() → 클라 MainScene::OnRoomExited() [선언]
	struct LeaveRoomResult { ErrorCode result = ErrorCode::Ok; RoomId roomId = 0; };

	// 사용: 클라 ShootingScene::SendMapLoaded() → 서버 Room::SetPlayerLoaded() [선언]
	struct MapLoaded { RoomId roomId = 0; };

	// 사용: 클라 ShootingScene::RequestReadyChange() → 서버 Room::SetPlayerReady() [선언]
	struct ReadyChangeRequest { RoomId roomId = 0; std::uint8_t ready = 0; };

	// 사용: 서버 Room::SendRoomState() → 클라 ShootingScene::OnRoomStateChanged() [선언] / 묶음: 참가자·준비·구역
	struct RoomState { RoomStateData room; };

	// 사용: 서버 Room::StartGameIfReady() → 클라 ShootingScene::OnGameStarted() [선언] / 묶음: 시간·규칙·플레이어, 표적 목록은 UDP로만 전달
	struct GameStarted
	{
		RoomVersion version;
		ServerTimeMs serverTimeMs = 0;
		ServerTimeMs startsAtMs = 0;
		ServerTimeMs endsAtMs = 0;
		MatchRules rules;
		std::vector<PlayerData> players;
	};

	// 사용: 클라 ShootingScene::SendShoot() → 서버 Room::RecvShoot(), Room::JudgeShoot() [선언] / 개별 발사 요청
	struct FireRequest
	{
		MatchContext context;
		MembershipId membershipId = 0;
		ShotId shotId = 0;
		Sequence inputSequence = 0;
		Mat4Data shooterTransform;
		Vec3Data origin;
	};

	// 사용: 서버 Room::SendShootResult() → 클라 ShootingScene::RecvShootResult() [선언] / 묶음: 발사자 토큰·명중 결과·점수·탄
	//       수, 표적 생성·삭제와 반동 적용 없음
	struct ShotResolved { RoomVersion version; SessionToken shooterToken{};

		ShotId shotId = 0;
		ShotOutcome outcome = ShotOutcome::Miss;
		PlayerStats shooterStats;
	};

	// 사용: 서버 StageController::SendMatchTime() → 클라 ShootingScene::UpdateMatchTime() [선언]
	struct MatchTime { MatchContext context; ServerTimeMs serverTimeMs = 0; ServerTimeMs endsAtMs = 0; };

	// 사용: 서버 StageController::SendGameResults() → 클라 ShootingScene::RecvGameResults(), ShootingScene::OnGameEnded() [선언]
	//       / 묶음: 순위·방 상태·준비 초기화
	struct GameResults
	{
		RoomStateData room;
		ServerTimeMs endedAtMs = 0;
		MatchEndReason reason = MatchEndReason::TimeLimit;
		std::vector<PlayerResult> players;
	};

	// 사용: 클라 NetworkClient::RegisterUDP() → 서버 NetworkServer::RegisterPlayerUDP() [선언]
	struct UdpRegisterRequest {};

	// 사용: 서버 NetworkServer::RegisterPlayerUDP() → 클라 NetworkClient::CompleteConnection() [선언]
	struct UdpRegisterResult { ErrorCode result = ErrorCode::Ok; Sequence registrationSequence = 0; };

	// 사용: 클라 ShootingScene::SendPlayerTransform() → 서버 Room::RecvPlayerInput(), Room::UpdatePlayerTransform() [선언] / 개별
	//       플레이어의 최신 변환 행렬
	struct PlayerTransform { Sequence inputSequence = 0; Mat4Data worldTransform; };

	// 사용: 서버 Room::SendTransformData() → 클라 ShootingScene::RecvTransformData() [선언] / 묶음: 전체 플레이어·본인 보정
	struct PlayerSnapshot
	{
		ServerTimeMs serverTimeMs = 0;
		Revision requiredRevision = 0;
		std::uint8_t playerCount = 0;
		std::array<PlayerTransformEntry, MaxPlayers> players{};

	};

	// 한 UDP 데이터그램에 생존 표적 전체를 담는다.
	struct TargetSnapshot
	{
		std::uint16_t targetCount = 0;
		std::array<TargetMotionData, MaxTargets> targets{};
	};

}
