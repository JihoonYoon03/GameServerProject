#include "../Server/Target.h"
#include "../Shared/Collision.h"
#include "../Shared/NetworkConversions.h"
#include <cmath>
#include <iostream>
#include <string_view>

bool RunProtocolPeerTests();

namespace
{
    bool RunCollisionTests()
    {
        Net::PlayerData player;
        player.worldTransform = Net::MakePlayerTransform({1.0f, 2.0f, 3.0f}, 0.0f, 0.0f);
        const Net::TargetMotionData target{1, {1.0f, 2.0f, -2.0f}, {}};
        const glm::mat4 playerWorld = Net::FromWire(player.worldTransform);
        const Ray shot{glm::vec3(playerWorld[3]), -3.0f * glm::vec3(playerWorld[2])};

        const Box localBounds{glm::vec3(-1.0f), glm::vec3(1.0f)};
        Transform targetTransform;
        targetTransform.position = Net::FromWire(target.position);
        const Sphere sphere = Collision::GetWorldSphere(localBounds, targetTransform);

        constexpr float tolerance = 0.0001f;
        float distance = 0.0f;
        if (!Collision::Raycast(shot, sphere, distance) || std::abs(distance - 4.0f) > tolerance)
        {
            std::cerr << "FAIL: sphere hit distance for a non-unit ray\n";
            return false;
        }
        if (Collision::Raycast({shot.origin, -shot.direction}, sphere, distance) ||
            Collision::Raycast({shot.origin, glm::vec3(0.0f)}, sphere, distance))
        {
            std::cerr << "FAIL: sphere behind the ray or zero direction\n";
            return false;
        }
        if (!Collision::Raycast({sphere.center, shot.direction}, sphere, distance) ||
            std::abs(distance - 1.0f) > tolerance)
        {
            std::cerr << "FAIL: sphere exit distance for a ray starting inside\n";
            return false;
        }

        targetTransform.rotation.y = 90.0f;
        targetTransform.scale = {2.0f, 1.0f, 0.5f};
        const Box worldBounds = Collision::GetWorldBox(localBounds, targetTransform);
        if (glm::length(worldBounds.min - glm::vec3(0.5f, 1.0f, -4.0f)) > tolerance ||
            glm::length(worldBounds.max - glm::vec3(1.5f, 3.0f, 0.0f)) > tolerance)
        {
            std::cerr << "FAIL: world bounds for a rotated and scaled box\n";
            return false;
        }
        if (!Collision::Raycast(shot, worldBounds, distance) || std::abs(distance - 3.0f) > tolerance ||
            Collision::Raycast({shot.origin + glm::vec3(1.0f, 0.0f, 0.0f), shot.direction}, worldBounds, distance))
        {
            std::cerr << "FAIL: box entry distance or parallel miss\n";
            return false;
        }

        std::cout << "PASS: world bounds and sphere/box raycasts\n";
        return true;
    }
}

// 서버 로직 테스트용 코드
// 표적 수명과 충돌 계산을 검증한 뒤, 기본 실행에서는 패킷 처리도 검사한다.
int main(int argc, char* argv[])
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

    std::cout << "PASS: spawn limit, movement, expiration\n";
    if (!RunCollisionTests())
        return 1;
    if (argc == 2 && std::string_view(argv[1]) == "--logic-only")
        return 0;
    return RunProtocolPeerTests() ? 0 : 1;
}
