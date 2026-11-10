#pragma once
// 클라이언트와 서버가 함께 사용하는 전송 데이터. 그래픽·운영체제 헤더에 의존하지 않는다.
// 세션 토큰은 접속별 플레이어 식별자이며, 비밀 인증값으로 사용하지 않는다.
// 구조체·벡터 메모리를 직접 보내지 않고 각 필드를 순서대로 직렬화한다.
// 행렬은 열 우선 단정밀도 실수 16개이며, 전송 시 각 값의 비트를 네트워크 바이트 순서로 변환한다.

//Shared 소스코드는 모두 이 헤더를 참조함. 여기에 공용 라이브러리 include 할 것
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <numeric>

namespace Net
{
	using RoomId = std::uint32_t;
	using MatchId = std::uint32_t;
	using MembershipId = std::uint32_t;
	using TargetId = std::uint32_t;
	using ShotId = std::uint32_t;
	using Sequence = std::uint32_t;
	using Revision = std::uint32_t;
	using ServerTimeMs = std::uint64_t;
	using SessionToken = std::array<std::uint8_t, 16>;
	inline constexpr std::size_t SessionTokenBytes = 16;

	//아래 함수는 크게 용도가 없지 않나
	bool IsValidSessionToken(const SessionToken& m_sessionToken);

	//뒤늦게 도착한 패킷 걸러내기
	bool IsNewerSequence(Sequence incoming, Sequence previous);

	inline constexpr std::uint8_t MaxPlayers = 4;
	inline constexpr std::uint8_t MinPlayersToStart = 2;
	inline constexpr std::uint8_t UnassignedArea = 255;
	inline constexpr std::uint16_t MaxTargets = 6;
	inline constexpr std::uint16_t MaxRooms = 64;
	inline constexpr std::uint32_t ServerTickRate = 30;
	inline constexpr float PlayerMoveSpeed = 10.0f / 3.6f;
	inline constexpr float PlayerHalfExtent = 0.25f;
	inline constexpr std::uint32_t MatchDurationMs = 120000;
	inline constexpr std::uint32_t TargetLifetimeMs = 3000;
	inline constexpr std::uint8_t MagazineCapacity = 6;
	inline constexpr float FireIntervalSeconds = 0.2f;
	inline constexpr float ReloadSeconds = 2.0f;
	inline constexpr float RecoilAnimSpeed = 1.5f;

	enum class ReadyState : std::uint8_t
	{
		Connecting = 0,
		Waiting = 1,
		Ready = 2,
		Playing = 3
	};

	enum class GameState : std::uint8_t
	{
		Preparing = 0,
		Playing = 1,
		Results = 2
	};

	enum class ErrorCode : std::uint16_t
	{
		Ok = 0,
		InvalidRequest = 1,
		VersionMismatch = 2,
		RoomNotFound = 3,
		RoomFull = 4,
		RoomPlaying = 5,
		AlreadyInRoom = 6,
		NotInRoom = 7,
		MapNotLoaded = 8,
		UdpNotRegistered = 9,
		StaleMatch = 10,
		NotPlaying = 11,
		DuplicateShot = 12,
		InvalidTransform = 13,
		InvalidSession = 14,
		ServerFull = 15
	};

	enum class ShotOutcome : std::uint8_t
	{
		Miss = 0,
		HitTarget = 1,
		BlockedByWorld = 2
	};

	enum class MatchEndReason : std::uint8_t
	{
		TimeLimit = 0,
		TooFewPlayers = 1,
		ServerStopped = 2
	};

	// 사용: Net::ToWire(glm::vec3), Net::FromWire(Vec3Data) [선언] / 패킷 전용, 연산은 glm::vec3
	struct Vec3Data { float x = 0.0f; float y = 0.0f; float z = 0.0f; };

	// 사용: Player::GetNetworkTransform(), Player::ApplyNetworkTransform(), Net::ToWire(glm::mat4),
	//       Net::FromWire(Mat4Data) [선언] / 위치·회전 통합 행렬
	struct Mat4Data
	{

		std::array<float, 16> elements{{
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		}};

	};

	// 사용: 서버 Room::AssignPlayerAreas(), Room::CheckPlayerCollision() [선언]
	struct BoxData { Vec3Data min; Vec3Data max; };

	// 사용: 클라 NetworkClient::UpdateNetwork(), 서버 Room::RecvPlayerInput(), Room::RecvShoot() [선언]
	struct MatchContext { RoomId roomId = 0; MatchId matchId = 0; };

	// 사용: 서버 Room::SendRoomState(), Room::SendShootResult() / 클라 ShootingScene::OnRoomStateChanged() [선언]
	struct RoomVersion { MatchContext context; Revision revision = 0; };

	// 사용: 서버 Room::AssignPlayerAreas(), Room::CheckPlayerCollision() / 클라 ShootingScene::OnRoomStateChanged() [선언]
	struct AreaData { std::uint8_t areaId = UnassignedArea; BoxData movementBounds; Mat4Data spawnTransform; };

	// 사용: 서버 StageController::StartGame(), Target::SpawnTargets() / 클라 ShootingScene::OnGameStarted() [선언]
	struct MatchRules
	{
		std::uint32_t durationMs = MatchDurationMs;
		std::uint16_t activeTargetCount = MaxTargets;
		std::uint16_t pointsPerHit = 1;
	};

	// 사용: 서버 Room::RecvShoot(), Room::JudgeShoot(), StageController::JudgeWhoWins() / 클라
	//       ShootingScene::RecvShootResult() [선언]
	struct PlayerStats { std::uint32_t score = 0; std::uint32_t shotsFired = 0; };

	// 사용: 서버 Room::SetPlayerLoaded(), Room::SetPlayerReady(), Room::UpdatePlayerTransform() / 클라
	//       ShootingScene::OnRoomStateChanged() [선언]
	struct PlayerData { SessionToken m_sessionToken{};

		Mat4Data worldTransform;
		std::uint8_t areaId = UnassignedArea;
		ReadyState ready = ReadyState::Connecting;
		std::uint8_t mapLoaded = 0;
		PlayerStats stats;
	};

	// 표적 이동은 월드 좌표와 초당 이동 벡터로 전송한다.
	struct TargetMotionData { Vec3Data position; Vec3Data velocity; };

	// 사용: 서버 RoomManager::SendRoomList() / 클라 MainScene::UpdateRoomList(), MainScene::ShowRoomList() [선언]
	struct RoomInfo
	{
		RoomId roomId = 0;
		std::uint8_t playerCount = 0;
		std::uint8_t capacity = MaxPlayers;
		GameState state = GameState::Preparing;
	};

	// 사용: 서버 StageController::JudgeWhoWins(), StageController::SendGameResults() / 클라 ShootingScene::RecvGameResults(),
	//       ShootingScene::ShowGameResult() [선언]
	struct PlayerResult { SessionToken m_sessionToken{};

		PlayerStats stats;
		std::uint8_t rank = 0;
	};

	// 사용: 서버 Room::SendRoomState() / 클라 ShootingScene::OnRoomStateChanged(), ShootingScene::OnGameEnded() [선언]
	struct RoomStateData
	{
		RoomVersion version;
		GameState state = GameState::Preparing;
		std::uint8_t capacity = MaxPlayers;
		MatchRules rules;
		std::vector<AreaData> vAreas;
		std::vector<PlayerData> vPlayers;
	};

	// 사용: 서버 Room::SendTransformData() / 클라 ShootingScene::RecvTransformData() [선언] / 위치·회전 통합 행렬
	struct PlayerTransformEntry { SessionToken m_sessionToken{};

		Mat4Data worldTransform;
		Sequence lastProcessedInput = 0;
	};

}
