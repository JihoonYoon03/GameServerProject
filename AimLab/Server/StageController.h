#pragma once
#include "../Shared/NetworkPackets.h"

namespace Server
{
    class StageController
    {
    public:
        void StartGame(const Net::MatchRules& rules, Net::ServerTimeMs startsAtMs);
        void UpdateMatch(Net::ServerTimeMs nowMs);
        // 점수 우선, 동점이면 사용 탄 수가 적은 순서. 두 값이 같으면 공동 순위이다.
        std::vector<Net::PlayerResult> JudgeWhoWins() const;
        void ChangeGameStatus(Net::GameState state);
        void SendMatchTime();
        void SendGameResults(Net::MatchEndReason reason);
        void ResetGame();
    };
}
