#include "pch.h"
#include "StageController.h"

bool StageController::Start(int stageNumber)
{
	if (state_ != StageState::Idle && state_ != StageState::Clear) return false;

	if (stageNumber < 1 || stageNumber > static_cast<int>(stages_.size())) return false;

	ResetValues(stageNumber);
	ruleIndex_ = stageIndex_;
	state_ = StageState::Ready;

	return true;
}

void StageController::ResetValues(int stageNumber)
{
	stageIndex_ = stageNumber - 1;
	score_ = 0;
	requiredScore_ = stages_[stageIndex_].requiredScore;
	remaining_ = stages_[stageIndex_].duration;
	stateTime_ = 0.0f;
	resetPending_ = true;
}

StageEvents StageController::Update(float deltaTime)
{
	StageEvents events;
	events.clearTargets = resetPending_;
	resetPending_ = false;

	if (deltaTime <= 0.0f || state_ == StageState::Idle) return events;

	// control.cpp와 같이 임계값을 먼저 확인하고, 미달일 때만 시간을 누적합니다.
	// 큰 지연을 임의로 따라잡아 여러 번 생성하지 않습니다.
	const float interval = state_ == StageState::Playing ? 1.0f : 2.0f;

	if (stateTime_ < interval)
	{
		stateTime_ += deltaTime;

		return events;
	}

	stateTime_ = 0.0f;

	switch (state_)
	{
	case StageState::Ready:
		state_ = StageState::Playing;
		break;
	case StageState::Playing:
		events.spawnCount = stages_[ruleIndex_].targetsPerSecond;
		--remaining_; // 마지막 초에도 먼저 표적을 생성합니다.

		if (remaining_ == 0)
		{
			if (score_ < requiredScore_)
			{
				ResetValues(1);
				state_ = StageState::GameOver;
			}
			else if (ruleIndex_ == 2)
			{
				state_ = StageState::Finishing;
			}
			else
			{
				// 원본 stage2는 stage3 표시로 초기화한 후 stage2 콜백을 다시 예약합니다.
				// 이번 작업은 구조 리팩토링이므로 이 기존 버그를 별도로 고치지 않습니다.
				ResetValues(ruleIndex_ + 2);
				ruleIndex_ = 1;
				state_ = StageState::Ready;
			}
		}

		break;
	case StageState::GameOver:
		state_ = StageState::Idle;
		break;
	case StageState::Finishing:
		score_ = 0;
		state_ = StageState::Clear;
		resetPending_ = true;
		break;
	case StageState::Clear:
		state_ = StageState::Idle;
		break;
	default:
		break;
	}

	events.clearTargets = events.clearTargets || resetPending_;
	resetPending_ = false;

	return events;
}
