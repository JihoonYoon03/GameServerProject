#pragma once
#include "../Shared/NetworkMessages.h"

namespace Server
{
    class RoomManager
    {
    public:
        void SendRoomList(const Net::SessionToken& recipient,
            const Net::RoomListRequest& request);
        void CreateGameRoom(const Net::SessionToken& creator);
        void JoinGameRoom(const Net::SessionToken& player,
            const Net::JoinRoomRequest& request);
        Net::ErrorCode CheckRoomJoin(Net::RoomId roomId) const;
        // 요청 전달 중지와 방 스레드 종료를 확인한 뒤 방 객체를 삭제한다.
        void DeleteRoom(Net::RoomId roomId);
        void UpdateRooms();
    };
}
