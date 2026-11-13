#include "pch.h"
#include "StageController.h"
#include "Target.h"

namespace
{
	constexpr Net::ServerTimeMs TargetSpawnIntervalMs = 500;
}

void StageController::StartGame(const Net::MatchRules& rules, Target& targets, Net::ServerTimeMs nowMs)
{
	targets.ClearTargets();

	startsAtMs_ = nowMs;
	endsAtMs_ = nowMs + rules.durationMs;
	lastUpdateAtMs_ = nowMs;
	nextTargetSpawnAtMs_ = nowMs + TargetSpawnIntervalMs;
	state_ = Net::GameState::Playing;
}

Net::GameState StageController::UpdateMatch(const Net::MatchRules& rules, Target& targets, Net::ServerTimeMs nowMs)
{
	if (state_ != Net::GameState::Playing) return state_; // 경기중이 아니면 리턴

	if (nowMs >= endsAtMs_) // 경기 종료 시간 도달 시
	{
		targets.ClearTargets(); // 모든 타겟 제거
		state_ = Net::GameState::Results; // 게임 상태를 결과로 변경
		return state_;
	}

	if (nowMs > lastUpdateAtMs_)
	{
		const float deltaSeconds = static_cast<float>(nowMs - lastUpdateAtMs_) / 1000.0f;
		targets.UpdateTargets(deltaSeconds, nowMs); // 타겟 갱신
		lastUpdateAtMs_ = nowMs; // 마지막 갱신 시간 업데이트
	}

	while (nowMs >= nextTargetSpawnAtMs_)
	{
		targets.SpawnTargets(rules, nowMs);
		nextTargetSpawnAtMs_ += TargetSpawnIntervalMs;
	}

	return state_;
}
