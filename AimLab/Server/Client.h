#pragma once
#include "../Shared/NetworkTypes.h"

namespace Server
{
    // 소켓·UDP 주소 보관은 접속 처리 구현 시 추가한다.
    class Client
    {
    public:
        Net::SessionToken sessionToken{};
        Net::RoomId roomId = 0;
        Net::MembershipId membershipId = 0;
        bool udpRegistered = false;
    };
}
