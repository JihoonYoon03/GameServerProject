#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "../Shared/NetworkMessages.h"
#include "Target.h"
#include "ClientPlayer.h"
#include "StageController.h"
#include <atomic>
#include <queue>

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

	Room();
	Room(Net::MatchContext context, Target& targets,
		std::vector<TargetRecipient> recipients, PacketSender sender);
	~Room();
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

	// The thread owner must wait for RoomProc to finish before destroying the room.
	static DWORD WINAPI RoomProc(LPVOID parameter);
	void RequestStop();
	void AddCommand(const Net::RoomCommand& command);

	bool UpdatePlayerTransform(const Net::SessionToken& player,
		const Net::PlayerTransform& input, Net::ServerTimeMs receivedAtMs);
	void SendTransformData();

	void RecvShoot(const Net::SessionToken& player, const Net::FireRequest& request);
	Net::ShotResolved JudgeShoot(const Net::SessionToken& player,
		const Net::FireRequest& request);
	void SendShootResult(const Net::ShotResolved& result);
	// 활성 표적 전체 목록을 동일 시점 기준으로 나눠 UDP 전송한다.
	void SendTargetPositions(Net::ServerTimeMs nowMs);
private:
	void Tick();
	void ProcessCommands();

	// 경기 상태는 방 스레드만 바꾼다.
	Net::RoomStateData m_state{};
	std::vector<ClientPlayer> m_vPlayers;
	Target targetController_;
	StageController stageController_;
	std::vector<Net::BoxData> m_vWorldBoxes;

	std::atomic<bool> stopRequested_{false};
	//방마다 임계 영역이 필요한 이유: 서버 메인 쓰레드가 패킷을 받아 방의 큐에 넣어주고,
	//방 쓰레드는 ProcessCommands에서 해당 큐를 사용하기 때문
	CRITICAL_SECTION commandCS_{}; // 사용 전 초기화하고 스레드 종료 대기 후 해제한다.
	std::queue<Net::RoomCommand> commandQueue_;

	Target* targets_ = &targetController_; // 주입된 표적 관리자는 방보다 오래 유지되어야 한다.
	std::vector<TargetRecipient> targetRecipients_;
	PacketSender sendPacket_;
	Net::Sequence targetSnapshotSequence_ = 0;
};

