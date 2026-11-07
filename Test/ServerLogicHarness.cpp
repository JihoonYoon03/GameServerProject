#include "../Server/Target.h"
#include <iostream>

// 서버 로직 테스트용 코드
// 현재는 타겟의 생성, 수명, 이동에 대한 테스트
int main()
{
    Target targets;
    Net::MatchRules rules;
    rules.activeTargetCount = 1;
    constexpr Net::ServerTimeMs spawnedAt = 1000;

    // 타겟 생성
    targets.SpawnTargets(rules, spawnedAt);
    targets.SpawnTargets(rules, spawnedAt);
    const auto before = targets.GetActiveTargets(spawnedAt);
    if (before.size() != 1)
    {
        std::cerr << "FAIL: spawn limit\n";
        return 1;
    }

    // 타겟 이동
    targets.UpdateTargets(1.0f, spawnedAt + 1000);
    const auto after = targets.GetActiveTargets(spawnedAt + 1000);
    if (after.size() != 1 || after.front().position.y <= before.front().position.y)
    {
        std::cerr << "FAIL: upward movement\n";
        return 1;
    }

    // 타겟 수명
    targets.UpdateTargets(2.0f, spawnedAt + 3000);
    if (!targets.GetActiveTargets(spawnedAt + 3000).empty())
    {
        std::cerr << "FAIL: expiration\n";
        return 1;
    }

    // 전부 통과 후 성공 메시지 출력
    std::cout << "PASS: spawn limit, movement, expiration\n";
    return 0;
}