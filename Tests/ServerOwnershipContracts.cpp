#include "../AimLab/Server/Room.h"
#include <type_traits>

static_assert(!std::is_copy_constructible_v<Server::Room>);
static_assert(!std::is_move_constructible_v<Server::Room>);
static_assert(std::is_default_constructible_v<Server::Room>);
static_assert(std::is_constructible_v<Server::Room, Server::Target&, Net::MatchContext,
    std::vector<Server::TargetRecipient>, Net::PacketSender>);
static_assert(std::is_same_v<decltype(Server::ClientPlayer::weapon), Server::ServerWeaponState>);
