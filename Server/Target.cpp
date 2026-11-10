#include "pch.h"
#include "Target.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float TargetLifetimeSeconds = Net::TargetLifetimeMs / 1000.0f;
}

void Target::SpawnTargets(const Net::MatchRules& rules, Net::ServerTimeMs nowMs)
{
	if (targets_.size() >= (std::min)(rules.activeTargetCount, Net::MaxTargets)) return;

	std::uniform_real_distribution<float> x(-5.0f, 5.0f);
	std::uniform_real_distribution<float> y(0.5f, 2.0f);
	std::uniform_real_distribution<float> z(-5.0f, -0.5f);
	std::uniform_int_distribution<int> horizontal(-1, 1);
	const float directionX = static_cast<float>(horizontal(random_));
	const float inverseLength = 1.0f / std::sqrt(directionX * directionX + 1.0f);

	Net::TargetMotionData motion;
	motion.position = {x(random_), y(random_), z(random_)};
	motion.velocity = {directionX * inverseLength, inverseLength, 0.0f};
	targets_.push_back({nextTargetId_++, motion, nowMs});
}

void Target::UpdateTargets(float deltaTime, Net::ServerTimeMs nowMs)
{
	if (deltaTime > 0.0f)
	{
		for (auto& target : targets_)
		{
			const float moveTime = (std::min)(deltaTime, TargetLifetimeSeconds - target.elapsedSeconds);
			auto& motion = target.motion;
			motion.position.x += motion.velocity.x * moveTime;
			motion.position.y += motion.velocity.y * moveTime;
			motion.position.z += motion.velocity.z * moveTime;
			target.elapsedSeconds += moveTime;
		}
	}

	targets_.erase(std::remove_if(targets_.begin(), targets_.end(), [nowMs](const ActiveTarget& target)
	{
		return target.elapsedSeconds >= TargetLifetimeSeconds ||
			(nowMs >= target.spawnedAtMs && nowMs - target.spawnedAtMs >= Net::TargetLifetimeMs);
	}), targets_.end());
}

std::vector<Net::TargetMotionData> Target::GetActiveTargets(Net::ServerTimeMs nowMs) const
{
	std::vector<Net::TargetMotionData> activeTargets;
	activeTargets.reserve(targets_.size());
	for (const auto& target : targets_)
	{
		if (target.elapsedSeconds < TargetLifetimeSeconds &&
			nowMs >= target.spawnedAtMs && nowMs - target.spawnedAtMs < Net::TargetLifetimeMs)
		{
			activeTargets.push_back(target.motion);
		}
	}
	return activeTargets;
}
