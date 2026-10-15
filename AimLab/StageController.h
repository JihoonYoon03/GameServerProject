#pragma once
#include "pch.h"

struct StageDefinition
{
	int duration = 30;
	int requiredScore = 20;
	int targetsPerSecond = 1;
};

enum class StageState
{
	Idle,
	Ready,
	Playing,
	GameOver,
	Finishing,
	Clear
};

struct StageEvents
{
	bool clearTargets = false;
	int	 spawnCount = 0;
};

// 렌더링과 표적 생성은 Scene에 맡기고 원본의 시간/점수 전이만 관리합니다.
class StageController
{
public:
	bool Start(int stageNumber);
	StageEvents Update(float deltaTime);

	void AddScore() { ++score_; }
	StageState GetState() const { return state_; }
	int GetStageNumber() const { return stageIndex_ + 1; }
	int GetScore() const { return score_; }
	int GetRequiredScore() const { return requiredScore_; }
	int GetRemainingSeconds() const { return remaining_; }

private:
	void ResetValues(int stageNumber);

	const std::array<StageDefinition, 3> stages_{{{30, 20, 1}, {30, 40, 2}, {30, 60, 3}}};
	StageState							 state_ = StageState::Idle;
	int									 stageIndex_ = 0;
	int									 ruleIndex_ = 0;
	int									 score_ = 0;
	int									 requiredScore_ = 0;
	int									 remaining_ = 0;
	float								 stateTime_ = 0.0f;
	bool								 resetPending_ = false;
};
