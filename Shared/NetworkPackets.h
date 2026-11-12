#pragma once
#include "NetworkTypes.h"

// TCP uses its frame length. UDP uses the received datagram length and message counts.
// PacketCodec writes fields in network byte order without copying object padding.
namespace Net
{
	inline constexpr std::uint16_t UdpMagic = 0x4347;
	inline constexpr std::uint32_t TcpHeaderBytes = 10;
	inline constexpr std::uint32_t MaxTcpPacketBytes = 65536;
	inline constexpr std::uint16_t UdpHeaderBytes = 40;
	inline constexpr std::uint16_t MaxUdpPacketBytes = 1200;
	inline constexpr std::uint16_t TargetSnapshotFixedBytes = UdpHeaderBytes + 2;
	inline constexpr std::uint16_t TargetMotionBytes = 24;
	static_assert(TargetSnapshotFixedBytes + MaxTargets * TargetMotionBytes <= MaxUdpPacketBytes);

	enum class TcpType : std::uint16_t
	{
		ConnectResult = 0x0001,
		RequestRejected = 0x0002,
		CreateRoomRequest = 0x0110,
		RoomEnterResult = 0x0112,
		LeaveRoomRequest = 0x0113,
		LeaveRoomResult = 0x0114,
		MapLoaded = 0x0201,
		RoomState = 0x0203,
		GameStarted = 0x0301,
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
		TcpType type = TcpType::ConnectResult;
		std::uint32_t requestId = 0;
	};

	// 사용: 서버 NetworkServer::RegisterPlayerUDP(), DispatchPacket() / 클라 NetworkClient::NetworkLoop() [선언]
	struct UdpHeader
	{
		std::uint16_t magic = UdpMagic;
		UdpType type = UdpType::RegisterRequest;
		SessionToken m_sessionToken{};

		Sequence sequence = 0;
		MatchContext context;
		MembershipId membershipId = 0;
		std::uint32_t serverTick = 0;
	};

	// 사용: 서버 NetworkServer::AcceptPlayer() → 클라 NetworkClient::ConnectServer() [선언]
	struct ConnectResult
	{
		static constexpr TcpType type = TcpType::ConnectResult;
		ErrorCode result = ErrorCode::Ok;
		SessionToken sessionToken{};
		std::uint16_t udpPort = 0;
		ServerTimeMs serverTimeMs = 0;
		std::uint16_t tickRate = static_cast<std::uint16_t>(ServerTickRate);
	};

	// 사용: 서버 NetworkServer::DispatchPacket(), RoomManager::JoinGameRoom(), Room::RecvShoot() → 클라
	//       NetworkClient::UpdateNetwork() [선언]
	struct RequestRejected
	{
		static constexpr TcpType type = TcpType::RequestRejected;
		ErrorCode reason = ErrorCode::InvalidRequest;
		MatchContext context;
	};



	// 사용: 클라 MainScene::RequestGameRoom() → 서버 RoomManager::CreateGameRoom() [선언]
	struct CreateRoomRequest
	{
		static constexpr TcpType type = TcpType::CreateRoomRequest;
	};


	// 사용: 서버 RoomManager::CreateGameRoom(), RoomManager::JoinGameRoom() → 클라 MainScene::JoinRoom() [선언]
	struct RoomEnterResult
	{
		static constexpr TcpType type = TcpType::RoomEnterResult;
		ErrorCode result = ErrorCode::Ok;
		MembershipId membershipId = 0;
		RoomStateData room;
	};

	// 사용: 클라 MainScene::ExitRoom() → 서버 Room::RemovePlayerFromRoom() [선언]
	struct LeaveRoomRequest
	{
		static constexpr TcpType type = TcpType::LeaveRoomRequest;
		RoomId roomId = 0;
	};

	// 사용: 서버 Room::RemovePlayerFromRoom() → 클라 MainScene::OnRoomExited() [선언]
	struct LeaveRoomResult
	{
		static constexpr TcpType type = TcpType::LeaveRoomResult;
		ErrorCode result = ErrorCode::Ok;
		RoomId roomId = 0;
	};

	// 사용: 클라 ShootingScene::SendMapLoaded() → 서버 Room::SetPlayerLoaded() [선언]
	struct MapLoaded
	{
		static constexpr TcpType type = TcpType::MapLoaded;
		RoomId roomId = 0;
	};


	// 사용: 서버 Room::SendRoomState() → 클라 ShootingScene::OnRoomStateChanged() [선언] / 묶음: 참가자·준비·구역
	struct RoomState
	{
		static constexpr TcpType type = TcpType::RoomState;
		RoomStateData room;
	};

	// 사용: 서버 Room::StartGameIfReady() → 클라 ShootingScene::OnGameStarted() [선언] / 묶음: 시간·규칙·플레이어, 표적 목록은 UDP로만 전달
	struct GameStarted
	{
		static constexpr TcpType type = TcpType::GameStarted;
		RoomVersion version;
		ServerTimeMs serverTimeMs = 0;
		ServerTimeMs startsAtMs = 0;
		ServerTimeMs endsAtMs = 0;
		MatchRules rules;
		std::vector<PlayerData> players;
	};





	// 사용: 클라 NetworkClient::RegisterUDP() → 서버 NetworkServer::RegisterPlayerUDP() [선언]
	struct UdpRegisterRequest
	{
		static constexpr UdpType type = UdpType::RegisterRequest;
	};

	// 사용: 서버 NetworkServer::RegisterPlayerUDP() → 클라 NetworkClient::CompleteConnection() [선언]
	struct UdpRegisterResult
	{
		static constexpr UdpType type = UdpType::RegisterResult;
		ErrorCode result = ErrorCode::Ok;
		Sequence registrationSequence = 0;
	};

	// 사용: 클라 ShootingScene::SendPlayerTransform() → 서버 NetworkServer::ReceiveUdp(), Room::UpdatePlayerTransform() [선언] / 개별
	//       플레이어의 최신 변환 행렬
	struct PlayerTransform
	{
		static constexpr UdpType type = UdpType::PlayerTransform;
		Sequence inputSequence = 0;
		PlayerPose pose;
	};

	// 사용: 서버 Room::SendTransformData() → 클라 ShootingScene::RecvTransformData() [선언] / 묶음: 전체 플레이어·본인 보정
	struct PlayerSnapshot
	{
		static constexpr UdpType type = UdpType::PlayerSnapshot;
		ServerTimeMs serverTimeMs = 0;
		Revision requiredRevision = 0;
		std::uint8_t playerCount = 0;
		std::array<PlayerTransformEntry, MaxPlayers> players{};
	};

	// 한 UDP 데이터그램에 생존 표적 전체를 담는다.
	struct TargetSnapshot
	{
		static constexpr UdpType type = UdpType::TargetSnapshot;
		std::uint16_t targetCount = 0;
		std::array<TargetMotionData, MaxTargets> targets{};
	};

}
