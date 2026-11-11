#include "pch.h"
#include "StageController.h"
#include "Target.h"

void StageController::StartGame(const Net::MatchRules& rules, Target& targets)
{
	const Net::ServerTimeMs nowMs = GetTickCount64(); // 시작 시간 저장
	const std::uint16_t initialTargetCount = rules.activeTargetCount < Net::MaxTargets
		? rules.activeTargetCount : Net::MaxTargets; // 초기 타겟의 개수, 설정한 타겟 개수 / 최대 타겟 개수 중 작은 값

	targets.ClearTargets();
	for (std::uint16_t index = 0; index < initialTargetCount; ++index)
	{
		targets.SpawnTargets(rules, nowMs);
	} // 타겟 스폰

	startsAtMs_ = nowMs;
	endsAtMs_ = nowMs + rules.durationMs;
	lastUpdateAtMs_ = nowMs;
	state_ = Net::GameState::Playing;
}

Net::GameState StageController::UpdateMatch(Net::ServerTimeMs nowMs, Target& targets)
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

	return state_;
}
