#pragma once
#include "NetworkTypes.h"

//서버와 클라 모두 같은 스테이지 배치 정보를 가짐
namespace ShootingRange
{
    inline constexpr float TargetHalfSize = 5.0f;
    inline constexpr float AreaInnerHalfSize = 2.0f;
    inline constexpr float WallHalfThickness = 0.1f;
    inline constexpr float WallHalfHeight = 0.375f;
    inline constexpr float FloorHalfThickness = 0.04f;
    inline constexpr float WallOffset = AreaInnerHalfSize + WallHalfThickness;
    inline constexpr float AreaOuterHalfSize = WallOffset + WallHalfThickness;
    inline constexpr float WallCenterY = 2.0f * FloorHalfThickness + WallHalfHeight;

    struct AreaPart
    {
        glm::vec3 offset;
        glm::vec3 halfSize;
        bool solid;
    };
    inline const std::array<AreaPart, 5> AreaParts{{
        {{0, FloorHalfThickness, 0}, {AreaOuterHalfSize, FloorHalfThickness, AreaOuterHalfSize}, false},
        {{-WallOffset, WallCenterY, 0}, {WallHalfThickness, WallHalfHeight, AreaInnerHalfSize}, true},
        {{WallOffset, WallCenterY, 0}, {WallHalfThickness, WallHalfHeight, AreaInnerHalfSize}, true},
        {{0, WallCenterY, -WallOffset}, {AreaOuterHalfSize, WallHalfHeight, WallHalfThickness}, true},
        {{0, WallCenterY, WallOffset}, {AreaOuterHalfSize, WallHalfHeight, WallHalfThickness}, true}
    }};
    inline const std::array<glm::vec3, 4> AreaCenters{{
        {9, 0, 0}, {-9, 0, 0}, {0, 0, -9}, {0, 0, 9}
    }};
    inline constexpr std::size_t SouthArea = 3;
}
