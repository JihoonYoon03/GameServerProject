#include "pch.h"
#include "Target.h"
#include "../Shared/Collision.h"
#include "../Shared/NetworkConversions.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
	constexpr float TargetLifetimeSeconds = Net::TargetLifetimeMs / 1000.0f;
}

void Target::SpawnTargets(const Net::MatchRules& rules, Net::ServerTimeMs nowMs)
{
	if (targets_.size() >= (std::min)(rules.activeTargetCount, Net::MaxTargets)) return;

void Target::SpawnTargets(Net::ServerTimeMs nowMs)
{
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

bool Target::TargetHits(const Net::Vec3Data& shooterPosition, const Net::Vec3Data& aimDirection)
{
	const Ray ray{Net::FromWire(shooterPosition), Net::FromWire(aimDirection)};
	const float directionLength = glm::length(ray.direction);
	if (!std::isfinite(ray.origin.x) || !std::isfinite(ray.origin.y) ||
		!std::isfinite(ray.origin.z) || !std::isfinite(directionLength) ||
		directionLength < 0.000001f)
	{
		return false;
	}

	const Net::ServerTimeMs nowMs = GetTickCount64();
	Net::TargetId nearestId = 0;
	float nearestDistance = std::numeric_limits<float>::infinity();
	// 모든 활성 표적을 검사하면서 광선과 표면이 만나는 거리를 비교한다.
	for (const auto& target : targets_)
	{
		if (!target.IsActiveAt(nowMs)) continue;

		const Sphere sphere{Net::FromWire(target.motion.position), TargetRadius};
		float distance = 0.0f;
		if (Collision::Raycast(ray, sphere, distance) && distance < nearestDistance)
		{
			nearestDistance = distance;
			nearestId = target.motion.targetId;
		}
	}
	if (nearestId == 0) return false;

	// 명중한 표적을 제거하고 줄어든 표적 수만큼 하나를 보충한다.
	ClearTarget(nearestId);
	SpawnTargets(nowMs);
	return true;
}

void Target::ClearTarget(Net::TargetId targetId)
{
	const auto target = std::find_if(targets_.begin(), targets_.end(), [targetId](const ActiveTarget& entry)
	{
		return entry.motion.targetId == targetId;
	});
	if (target != targets_.end()) targets_.erase(target);
}

void Target::ClearTargets()
{
	targets_.clear();
}

bool Target::ActiveTarget::IsActiveAt(Net::ServerTimeMs nowMs) const
{
	return elapsedSeconds < TargetLifetimeSeconds &&
		nowMs >= spawnedAtMs && nowMs - spawnedAtMs < TargetLifetimeMs;
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
