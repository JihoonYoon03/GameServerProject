#pragma once
#include "../Shared/NetworkTypes.h"
#include <random>
#include <vector>


class Target
{
public:
	// 경기 진행 중 표적 생성 주기마다 한 번 호출한다.
	void SpawnTargets(const Net::MatchRules& rules, Net::ServerTimeMs nowMs);
	void UpdateTargets(float deltaTime, Net::ServerTimeMs nowMs);
	Net::ShotOutcome TargetHits(const Net::Vec3Data& shooterPosition, const Net::Vec3Data& aimDirection,
		const std::vector<Net::BoxData>& worldBoxes, const Net::MatchRules& rules, Net::ServerTimeMs nowMs);
	void ClearTargets();
	std::vector<Net::TargetMotionData> GetActiveTargets(Net::ServerTimeMs nowMs) const;

private:
	struct ActiveTarget
	{
		bool IsActiveAt(Net::ServerTimeMs nowMs) const;

		Net::TargetId targetId;
		Net::TargetMotionData motion;
		Net::ServerTimeMs spawnedAtMs;
		float elapsedSeconds = 0.0f;
	};

	std::vector<ActiveTarget> targets_;
	std::mt19937 random_{std::random_device{}()};
	Net::TargetId nextTargetId_ = 1;
};
