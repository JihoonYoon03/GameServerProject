#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include "../Shared/NetworkMessages.h"
#include "Target.h"
#include "ClientPlayer.h"
#include "StageController.h"
#include <atomic>
#include <queue>
#include <functional>
#include <utility>

// 경기 상태는 이 방의 스레드에서만 변경한다. 외부 요청은 AddCommand()로 전달한다.
class Client;
class RoomManager;

class Room
{
public:
	// 수신 대상은 모두 UDP 등록을 완료하고 이 방에 속한 참가자이다.
	struct TargetRecipient
	{
		Net::SessionToken sessionToken{};
		Net::MembershipId membershipId = 0;
	};
	using PacketSender = Net::PacketSender;

	Room() = default;
	Room(Net::MatchContext context, Target& targets,
		std::vector<TargetRecipient> recipients, PacketSender sender)
		: targetContext_(context), targets_(&targets),
		  targetRecipients_(std::move(recipients)), sendPacket_(std::move(sender))
	{
	}
	// 방의 상태와 잠금 저장 공간은 복사하거나 이동하지 않는다.
	Room(const Room&) = delete;
	Room& operator=(const Room&) = delete;
	Room(Room&&) = delete;
	Room& operator=(Room&&) = delete;

	void AssignPlayerAreas();
	void SetPlayerLoaded(const Net::SessionToken& player);
	void RemovePlayerFromRoom(const Net::SessionToken& player);
	void SetPlayerReady(const Net::SessionToken& player, bool ready);
	void SendRoomState();
	bool AreAllPlayersReady() const;
	void StartGameIfReady();
	void Tick();
	void StartRoomThread();
	void AddCommand(const Net::RoomCommand& command);
	void ProcessCommands();
	void StopRoom();
	void RecvPlayerInput(const Net::SessionToken& player,
		const Net::PlayerTransform& input);
	bool UpdatePlayerTransform(const Net::SessionToken& player,
		const Net::PlayerTransform& input);
	void CheckPlayerCollision(const Net::SessionToken& player);
	void SendTransformData();
	void RecvShoot(const Net::SessionToken& player, const Net::FireRequest& request);
	Net::ShotResolved JudgeShoot(const Net::SessionToken& player,
		const Net::FireRequest& request);
	void SendShootResult(const Net::ShotResolved& result);
	// 활성 표적 전체 목록을 동일 시점 기준으로 나눠 UDP 전송한다.
	void SendTargetPositions(SOCKET udpSocket, const std::vector<Client>& clients,
		Net::ServerTimeMs nowMs);
private:
	static void WriteUInt16(std::vector<std::uint8_t>& bytes, std::uint16_t value);
	static void WriteUInt32(std::vector<std::uint8_t>& bytes, std::uint32_t value);
	static void WriteUInt64(std::vector<std::uint8_t>& bytes, std::uint64_t value);
	static void WriteFloat(std::vector<std::uint8_t>& bytes, float value);

	// 경기 상태는 방 스레드만 바꾼다. 생성자와 스레드 수명 처리는 후속 일정에서 구현한다.
	Net::RoomStateData state_{};
	std::vector<ClientPlayer> players_;
	Target targetController_;
	StageController stageController_;
	RoomManager* manager_ = nullptr;
	std::vector<Net::BoxData> worldBoxes_;
	static DWORD WINAPI RoomProc(LPVOID parameter);
	HANDLE roomThread_ = nullptr;
	std::atomic<bool> stopRequested_{false};
	CRITICAL_SECTION commandCS_{}; // 사용 전 초기화하고 스레드 종료 대기 후 해제한다.
	std::queue<Net::RoomCommand> commandQueue_;
	Net::MatchContext targetContext_{};
	Target* targets_ = &targetController_; // 주입된 표적 관리자는 방보다 오래 유지되어야 한다.
	std::vector<TargetRecipient> targetRecipients_;
	PacketSender sendPacket_;
	Net::Sequence targetSnapshotSequence_ = 0;
};

