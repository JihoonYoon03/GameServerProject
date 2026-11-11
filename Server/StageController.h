#pragma once
#include "../Shared/NetworkPackets.h"

class Target;

class StageController
{
public:
	void StartGame(const Net::MatchRules& rules, Target& targets);
	Net::GameState UpdateMatch(Net::ServerTimeMs nowMs, Target& targets);
	// 점수 우선, 동점이면 사용 탄 수가 적은 순서. 두 값이 같으면 공동 순위이다.
	std::vector<Net::PlayerResult> JudgeWhoWins() const;
	void ChangeGameStatus(Net::GameState state);
	void SendMatchTime();
	void SendGameResults(Net::MatchEndReason reason);
	void ResetGame();

private:
	Net::ServerTimeMs startsAtMs_ = 0;
	Net::ServerTimeMs endsAtMs_ = 0;
	Net::ServerTimeMs lastUpdateAtMs_ = 0;
	Net::GameState state_ = Net::GameState::Preparing;
};

