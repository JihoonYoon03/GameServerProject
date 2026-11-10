#include "pch.h"
#include "Target.h"
#include "../Shared/Collision.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
	constexpr float TargetLifetimeSeconds = Net::TargetLifetimeMs / 1000.0f;
	constexpr float TargetRadius = 0.15f;
}

void Target::SpawnTargets(const Net::MatchRules& rules, Net::ServerTimeMs nowMs)
{
	if (targets_.size() >= (std::min)(rules.activeTargetCount, Net::MaxTargets)) return;
	std::uniform_real_distribution<float> x(-5.0f, 5.0f);
	std::uniform_real_distribution<float> y(0.5f, 2.0f);
	std::uniform_real_distribution<float> z(-5.0f, -0.5f);
	std::uniform_int_distribution<int> horizontal(-1, 1);
	float directionX = static_cast<float>(horizontal(random_));
	float inverseLength = 1.0f / std::sqrt(directionX * directionX + 1.0f);

	Net::TargetMotionData motion;
	motion.position = {x(random_), y(random_), z(random_)};
	motion.velocity = {directionX * inverseLength, inverseLength, 0.0f};
	targets_.push_back({nextTargetId_++, motion, nowMs});
}

void Target::UpdateTargets(float deltaTime, Net::ServerTimeMs nowMs)
{
	if (deltaTime > 0.0f)
	{
		for (ActiveTarget& target : targets_)
		{
			float moveTime = (std::min)(deltaTime, TargetLifetimeSeconds - target.elapsedSeconds);
			Net::TargetMotionData& motion = target.motion;
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

Net::ShotOutcome Target::TargetHits(const glm::vec3& shooterPosition, const glm::vec3& aimDirection,
	const std::vector<Net::BoxData>& worldBoxes, const Net::MatchRules& rules, Net::ServerTimeMs nowMs)
{
	const Ray ray{shooterPosition, aimDirection};
	float directionLength = glm::length(ray.direction);
	if (!std::isfinite(ray.origin.x) || !std::isfinite(ray.origin.y) ||
		!std::isfinite(ray.origin.z) || !std::isfinite(directionLength) ||
		directionLength < 0.000001f)
	{
		return Net::ShotOutcome::Miss;
	}

	float nearestDistance = std::numeric_limits<float>::infinity();
	Net::ShotOutcome outcome = Net::ShotOutcome::Miss;
	for (const Net::BoxData& worldBox : worldBoxes)
	{
		const Box obstacle{worldBox.min, worldBox.max};
		float distance = 0.0f;
		if (Collision::Raycast(ray, obstacle, distance) && distance < nearestDistance)
		{
			nearestDistance = distance;
			outcome = Net::ShotOutcome::BlockedByWorld;
		}
	}

	auto nearestTarget = targets_.end();
	for (auto target = targets_.begin(); target != targets_.end(); ++target)
	{
		if (!target->IsActiveAt(nowMs)) continue;

		const Sphere sphere{target->motion.position, TargetRadius};
		float distance = 0.0f;
		if (Collision::Raycast(ray, sphere, distance) && distance < nearestDistance)
		{
			nearestDistance = distance;
			nearestTarget = target;
		}
	}
	if (nearestTarget == targets_.end()) return outcome;

	targets_.erase(nearestTarget);
	SpawnTargets(rules, nowMs);
	return Net::ShotOutcome::HitTarget;
}

void Target::ClearTargets()
{
	targets_.clear();
}

bool Target::ActiveTarget::IsActiveAt(Net::ServerTimeMs nowMs) const
{
	return elapsedSeconds < TargetLifetimeSeconds &&
		nowMs >= spawnedAtMs && nowMs - spawnedAtMs < Net::TargetLifetimeMs;
}

std::vector<Net::TargetMotionData> Target::GetActiveTargets(Net::ServerTimeMs nowMs) const
{
	std::vector<Net::TargetMotionData> activeTargets;
	activeTargets.reserve(targets_.size());
	for (const ActiveTarget& target : targets_)
	{
		if (target.IsActiveAt(nowMs))
		{
			activeTargets.push_back(target.motion);
		}
	}
	return activeTargets;
}
