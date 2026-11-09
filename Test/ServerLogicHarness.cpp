#include "../Server/Target.h"
#include <iostream>

bool RunProtocolPeerTests();

// 서버 로직 테스트용 코드
// 표적의 생성·이동·수명과 테스트 클라이언트의 패킷 처리를 검증합니다.
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

    // 표적 테스트 결과를 출력한 뒤 네트워크 테스트를 실행합니다.
    std::cout << "PASS: spawn limit, movement, expiration\n";
    return RunProtocolPeerTests() ? 0 : 1;
}
