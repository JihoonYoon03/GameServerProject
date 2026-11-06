#pragma once
#include "../Shared/NetworkTypes.h"

namespace Server
{
    struct ServerWeaponState
    {
        std::uint8_t ammo = Net::MagazineCapacity;
        bool reloading = false;
        Net::ServerTimeMs nextFireAtMs = 0;
        Net::ServerTimeMs reloadEndsAtMs = 0;
    };
}
