#pragma once
#include "../Shared/NetworkTypes.h"
#include <random>
#include <vector>


class Target
{
public:
	// Call once per spawn interval during an active match.
	void SpawnTargets(const Net::MatchRules& rules, Net::ServerTimeMs nowMs);
	void UpdateTargets(float deltaTime, Net::ServerTimeMs nowMs);
	bool TargetHits(Net::TargetId targetId, Net::ServerTimeMs nowMs);
	void ClearTargets();

private:
	struct ActiveTarget
	{
		Net::TargetMotionData motion;
		Net::ServerTimeMs spawnedAtMs;
		float elapsedSeconds = 0.0f;
	};

	std::vector<ActiveTarget> targets_;
	std::mt19937 random_{std::random_device{}()};
	Net::TargetId nextTargetId_ = 1;
};
