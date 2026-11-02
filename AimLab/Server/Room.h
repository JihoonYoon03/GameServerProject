#pragma once
#include "../Shared/NetworkMessages.h"

namespace Server
{
    // 경기 상태는 이 방의 스레드에서만 변경한다. 외부 요청은 AddCommand()로 전달한다.
    class Room
    {
    public:
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
    };
}
