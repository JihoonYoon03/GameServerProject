#include "../Server/Room.h"
#include <type_traits>

static_assert(!std::is_copy_constructible_v<Room>);
static_assert(!std::is_move_constructible_v<Room>);
static_assert(std::is_default_constructible_v<Room>);
static_assert(std::is_constructible_v<Room, Net::MatchContext, Target&,
    std::vector<Room::TargetRecipient>, Net::PacketSender>);
static_assert(std::is_same_v<decltype(ClientPlayer::weapon), ServerWeaponState>);
