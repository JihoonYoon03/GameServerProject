#pragma once
#include "../Shared/NetworkTypes.h"

namespace Server
{
    class ClientPlayer
    {
    public:
        Net::PlayerData state;
        Net::Sequence lastProcessedInput = 0;
        Net::ShotId lastProcessedShot = 0;
    };
}
