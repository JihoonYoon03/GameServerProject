#pragma once
#include "../Shared/NetworkPackets.h"

class ClientPlayer
{
public:
	bool ApplyMovement(const Net::PlayerTransform& input, const Net::AreaData& area,
		const std::vector<Net::BoxData>& obstacles, Net::ServerTimeMs receivedAtMs);

	Net::PlayerData		m_PlayerData;

	Net::Sequence		m_nLastProcessedInput = 0;
	Net::ServerTimeMs	m_nLastMovementTimeMs = 0;
	bool				m_bHasProcessedInput = false;
};

