#include "pch.h"
#include "Target.h"
#include "../Shared/Collision.h"
#include "../Shared/ShootingRange.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
	constexpr float TargetLifetimeSeconds = Net::TargetLifetimeMs / 1000.0f;
}

void Target::SpawnTargets(const Net::MatchRules& rules, Net::ServerTimeMs nowMs, Net::TargetKind kind)
{
	const std::array<std::uint16_t, 3> limits{{
		(std::min)(rules.activeTargetCount, Net::MovingTargetLimit), Net::OrbitTargetCount, Net::StationaryTargetCount
	}};
	if (kind > Net::TargetKind::Stationary) return;
	std::size_t count = std::count_if(targets_.begin(), targets_.end(), [kind](const ActiveTarget& target)
	{
		return target.motion.kind == kind;
	});
	if (count >= limits[static_cast<std::size_t>(kind)]) return;

	std::uniform_real_distribution<float> x(-ShootingRange::TargetHalfSize, ShootingRange::TargetHalfSize);
	std::uniform_real_distribution<float> y(0.5f, 2.0f);
	std::uniform_real_distribution<float> z(-ShootingRange::TargetHalfSize, ShootingRange::TargetHalfSize);

	ActiveTarget target{nextTargetId_++, {}, nowMs};
	target.motion.kind = kind;
	// Y는 0.5 이상으로 생성해 반지름 0.3인 표적이 바닥에 묻히지 않게 한다.
	target.motion.position = {0.0f, y(random_), 0.0f};
	if (kind != Net::TargetKind::Orbiting)
	{
		target.motion.position.x = x(random_);
		target.motion.position.z = z(random_);
	}
	if (kind == Net::TargetKind::Moving)
	{
		std::uniform_int_distribution<int> horizontal(-1, 1);
		float directionX = static_cast<float>(horizontal(random_));
		float directionZ = static_cast<float>(horizontal(random_));
		float inverseLength = 1.0f / std::sqrt(directionX * directionX + 1.0f + directionZ * directionZ);
		target.motion.velocity = {directionX * inverseLength, inverseLength, directionZ * inverseLength};
	}
	else if (kind == Net::TargetKind::Orbiting)
	{
		std::uniform_real_distribution<float> angle(0.0f, glm::radians(360.0f));
		target.initialOrbitAngle = angle(random_);
		target.orbitCenter = target.motion.position;
		float cosine = std::cos(target.initialOrbitAngle);
		float sine = std::sin(target.initialOrbitAngle);
		target.motion.position += ShootingRange::OrbitRadius * glm::vec3(cosine, 0.0f, sine);
		target.motion.velocity = ShootingRange::OrbitRadius * ShootingRange::OrbitAngularSpeed * glm::vec3(-sine, 0.0f, cosine);
	}
	targets_.push_back(target);
}

void Target::UpdateTargets(float deltaTime, const Net::MatchRules& rules, Net::ServerTimeMs nowMs)
{
	if (deltaTime > 0.0f)
	{
		for (ActiveTarget& target : targets_)
		{
			Net::TargetMotionData& motion = target.motion;
			if (motion.kind == Net::TargetKind::Stationary) continue;
			if (motion.kind == Net::TargetKind::Orbiting)
			{
				// 생성 시각부터의 누적 회전량으로 세 바퀴의 수명을 계산한다.
				target.elapsedSeconds = nowMs >= target.spawnedAtMs
					? static_cast<float>(nowMs - target.spawnedAtMs) / 1000.0f : 0.0f;
				float angle = target.initialOrbitAngle + ShootingRange::OrbitAngularSpeed * target.elapsedSeconds;
				float cosine = std::cos(angle);
				float sine = std::sin(angle);
				motion.position = target.orbitCenter + ShootingRange::OrbitRadius * glm::vec3(cosine, 0.0f, sine);
				motion.velocity = ShootingRange::OrbitRadius * ShootingRange::OrbitAngularSpeed * glm::vec3(-sine, 0.0f, cosine);
			}
			else
			{
				float moveTime = (std::min)(deltaTime, TargetLifetimeSeconds - target.elapsedSeconds);
				motion.position += motion.velocity * moveTime;
				target.elapsedSeconds += moveTime;
			}
		}
	}

	std::size_t expiredOrbitTargets = 0;
	targets_.erase(std::remove_if(targets_.begin(), targets_.end(), [nowMs, &expiredOrbitTargets](const ActiveTarget& target)
	{
		if (target.IsActiveAt(nowMs)) return false;
		if (target.motion.kind == Net::TargetKind::Orbiting) ++expiredOrbitTargets;
		return true;
	}), targets_.end());

	// 세 바퀴를 마친 파란 표적은 제거 직후 같은 종류로 보충한다.
	for (std::size_t index = 0; index < expiredOrbitTargets; ++index)
		SpawnTargets(rules, nowMs, Net::TargetKind::Orbiting);
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

		float radius = target->motion.kind == Net::TargetKind::Stationary
			? ShootingRange::StationaryTargetRadius : ShootingRange::TargetRadius;
		const Sphere sphere{target->motion.position, radius};
		float distance = 0.0f;
		if (Collision::Raycast(ray, sphere, distance) && distance < nearestDistance)
		{
			nearestDistance = distance;
			nearestTarget = target;
		}
	}
	if (nearestTarget == targets_.end()) return outcome;

	Net::TargetKind kind = nearestTarget->motion.kind;
	targets_.erase(nearestTarget);
	SpawnTargets(rules, nowMs, kind);
	return Net::ShotOutcome::HitTarget;
}

void Target::ClearTargets()
{
	targets_.clear();
}

bool Target::ActiveTarget::IsActiveAt(Net::ServerTimeMs nowMs) const
{
	if (motion.kind == Net::TargetKind::Stationary) return true;
	Net::ServerTimeMs lifetimeMs = motion.kind == Net::TargetKind::Orbiting
		? Net::OrbitLifetimeMs : Net::TargetLifetimeMs;
	return elapsedSeconds < lifetimeMs / 1000.0f &&
		nowMs >= spawnedAtMs && nowMs - spawnedAtMs < lifetimeMs;
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
