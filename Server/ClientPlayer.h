#pragma once
#include "../Shared/NetworkPackets.h"
#include "ServerWeaponState.h"

class ClientPlayer
{
public:
	bool ApplyMovement(const Net::PlayerTransform& input, const Net::AreaData& area,
		const std::vector<Net::BoxData>& obstacles, Net::ServerTimeMs receivedAtMs);

	Net::PlayerData state;
	Net::Sequence lastProcessedInput = 0;
	Net::ShotId lastProcessedShot = 0;
	bool hasProcessedInput = false;
	Net::Mat4Data lastProcessedTransform;
	Net::ServerTimeMs lastMovementBudgetMs = 0;
	ServerWeaponState weapon;
};

