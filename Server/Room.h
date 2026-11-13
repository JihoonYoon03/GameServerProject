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
	Room(Net::RoomId roomId, PacketSender sender);
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

	void SendRoomState();
	void StartGameIfReady();

	// 시작, 종료 대기, 종료 확인은 메인 스레드에서만 호출한다.
	bool StartRoomThread();
	void StopRoom();
	bool IsThreadFinished() const;
	static DWORD WINAPI RoomProc(LPVOID parameter);
	// 방 스레드 안에서는 대기하지 않고 종료만 요청한다.
	void RequestStop();
	bool AddCommand(const Net::RoomCommand& command);
	std::queue<Net::RoomManagementEvent> TakeStateEvents();

	bool UpdatePlayerTransform(const Net::SessionToken& player,
		const Net::PlayerTransform& input, Net::ServerTimeMs receivedAtMs);
	// 방 스레드에서 전체 플레이어의 서버 확정 위치와 마지막 처리 입력 순번을 송신한다.
	void SendTransformData();

	// 활성 표적 전체 목록을 동일 시점 기준으로 나눠 UDP 전송한다.
	void SendTargetPositions(Net::ServerTimeMs nowMs);
private:
	void Tick();
	void ProcessCommands();
	void ProcessCommand(const Net::RoomCommand& command);
	void EnterPlayer(const Net::RoomCommand& command, const Net::RoomEnterCommand& enter);
	void PublishStateEvent(Net::RoomManagementEventType type);

	// 경기 상태는 방 스레드만 바꾼다.
	Net::RoomStateData m_state{};
	std::vector<ClientPlayer> m_vPlayers;
	Target targetController_;
	StageController stageController_;
	std::vector<Net::BoxData> m_vWorldBoxes;

	HANDLE roomThread_ = nullptr;
	std::atomic<bool> stopRequested_{false};
	//방마다 임계 영역이 필요한 이유: 서버 메인 쓰레드가 패킷을 받아 방의 큐에 넣어주고,
	//방 쓰레드는 ProcessCommands에서 해당 큐를 사용하기 때문
	CRITICAL_SECTION commandCS_{}; // 사용 전 초기화하고 스레드 종료 대기 후 해제한다.
	std::queue<Net::RoomCommand> commandQueue_;
	std::queue<Net::RoomManagementEvent> m_qStateEvents;

	Target* targets_ = &targetController_; // 주입된 표적 관리자는 방보다 오래 유지되어야 한다.
	std::vector<TargetRecipient> targetRecipients_;
	PacketSender m_SendPacket;
	//플레이어·표적 스냅샷은 같은 UDP 순번을 사용한다.
	Net::Sequence m_nSnapshotSequence = 0;
};

