#pragma once
#include "../Shared/NetworkTypes.h"


class Target
{
public:
	void SpawnTargets(const Net::MatchRules& rules, Net::ServerTimeMs nowMs);
	void UpdateTargets(float deltaTime, Net::ServerTimeMs nowMs);
	bool TargetHits(Net::TargetId targetId, Net::ServerTimeMs nowMs);
	void ClearTargets();
};
