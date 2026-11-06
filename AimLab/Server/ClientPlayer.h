#pragma once
#include "../Shared/NetworkTypes.h"
#include "ServerWeaponState.h"

namespace Server
{
    class ClientPlayer
    {
    public:
        Net::PlayerData state;
        Net::Sequence lastProcessedInput = 0;
        Net::ShotId lastProcessedShot = 0;
        bool hasProcessedInput = false;
        Net::Mat4Data lastProcessedTransform;
        Net::ServerTimeMs lastMovementBudgetMs = 0;
        ServerWeaponState weapon;
    };
}
