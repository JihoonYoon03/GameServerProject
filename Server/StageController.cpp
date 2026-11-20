#include "pch.h"
#include "StageController.h"
#include "Target.h"
#include <algorithm>

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

std::vector<Net::PlayerResult> StageController::JudgeWhoWins(const std::vector<Net::PlayerData>& players) const
{
	std::vector<Net::PlayerResult> results;
	results.reserve(players.size());
	for (const Net::PlayerData& player : players)
		results.push_back({player.m_sessionToken, player.stats});

	// 사격 횟수가 홀수이면 0.5점 단위까지 반영한다.
	std::stable_sort(results.begin(), results.end(), [](const Net::PlayerResult& left, const Net::PlayerResult& right)
	{
		double leftScore = static_cast<double>(left.stats.score) - left.stats.shotsFired / 2.0;
		double rightScore = static_cast<double>(right.stats.score) - right.stats.shotsFired / 2.0;
		return leftScore > rightScore;
	});

	// 동점자는 공동 순위를 부여한다. 예를 들어 1, 1, 3위로 표시한다.
	double previousScore = 0.0;
	std::uint8_t rank = 0;
	for (std::size_t index = 0; index < results.size(); ++index)
	{
		double score = static_cast<double>(results[index].stats.score) - results[index].stats.shotsFired / 2.0;
		if (index == 0 || score != previousScore)
			rank = static_cast<std::uint8_t>(index + 1);
		results[index].rank = rank;
		previousScore = score;
	}
	return results;
}
