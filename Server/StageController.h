#pragma once
#include "../Shared/NetworkTypes.h"

class Target;

class StageController
{
public:
	void StartGame(const Net::MatchRules& rules, Target& targets);
	Net::GameState UpdateMatch(Net::ServerTimeMs nowMs, Target& targets);

private:
	Net::ServerTimeMs startsAtMs_ = 0;
	Net::ServerTimeMs endsAtMs_ = 0;
	Net::ServerTimeMs lastUpdateAtMs_ = 0;
	Net::GameState state_ = Net::GameState::Preparing;
};

