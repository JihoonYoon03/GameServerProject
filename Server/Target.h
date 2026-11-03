#pragma once
#include "../Shared/NetworkTypes.h"

namespace Server
{
    class Target
    {
    public:
        void SpawnTargets(const Net::MatchRules& rules, Net::ServerTimeMs nowMs);
        void UpdateTargets(float deltaTime, Net::ServerTimeMs nowMs);
        bool TargetHits(Net::TargetId targetId, Net::ServerTimeMs nowMs);
        void ClearTargets();
    };
}
