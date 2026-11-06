#pragma once
#include "../Shared/NetworkMessages.h"
#include "ClientPlayer.h"
#include "StageController.h"
#include "Target.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <atomic>
#include <queue>

namespace Server
{
    class RoomManager;
    using PacketSender = Net::PacketSender;
    struct TargetRecipient
    {
        Net::SessionToken sessionToken{};
        Net::MembershipId membershipId = 0;
    };

    // 경기 상태는 이 방의 스레드에서만 변경한다. 외부 요청은 AddCommand()로 전달한다.
    class Room
    {
    public:
        // 생성자의 잠금 초기화와 종료 시 스레드 대기 구현은 11/9에 진행한다.
        Room();
        Room(Target& targets, Net::MatchContext context,
            std::vector<TargetRecipient> recipients, PacketSender sender);
        ~Room();
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
        void SendTargetPositions();

    private:
        static DWORD WINAPI RoomProc(LPVOID parameter);
        // 경기 상태는 방 스레드만 바꾼다. 두 포인터는 외부 객체를 소유하지 않는다.
        Net::RoomStateData state_{};
        std::vector<ClientPlayer> players_;
        Target targetController_;
        Target* targets_ = &targetController_; // 주입받은 표적 관리자는 방보다 오래 유지되어야 한다.
        StageController stageController_;
        RoomManager* manager_ = nullptr;
        std::vector<Net::BoxData> worldBoxes_;
        Net::MatchContext targetContext_{};
        std::vector<TargetRecipient> targetRecipients_;
        PacketSender sendPacket_{};
        Net::Sequence targetSnapshotSequence_ = 0;
        HANDLE roomThread_ = nullptr;
        std::atomic<bool> stopRequested_{false};
        CRITICAL_SECTION commandCS_{}; // 생성자에서 초기화하고 스레드 종료 대기 후 해제한다.
        std::queue<Net::RoomCommand> commandQueue_;
    };
}
