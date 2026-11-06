#pragma once
#include "../Shared/NetworkTypes.h"

// 家南·UDP 林家 焊包篮 立加 贸府 备泅 矫 眠啊茄促.
class Client
{
public:
    Net::SessionToken   sessionToken{};

    SOCKET              TCPSocket = INVALID_SOCKET;
    sockaddr_in         UDPAddress{};
    bool                UDPRegistered = false;

    Net::RoomId         roomId = 0;
    Net::MembershipId   membershipId = 0;
};
