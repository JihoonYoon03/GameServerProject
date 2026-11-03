#pragma once
#include "NetworkTypes.h"
#include <GL/glm/glm.hpp>

// 변환 함수 선언. 행렬 연산 라이브러리는 이 헤더에서 참조하고 공용 전송 데이터에서는 분리한다.
// 플레이어 행렬은 위치 이동·좌우 회전·상하 회전 순서로 구성하고, 크기는 1로 고정한다.
// 눈높이 보정·모델 크기·본인 총 반동은 전송 행렬에 포함하지 않는다.
namespace Net
{
    Vec3Data ToWire(const glm::vec3& value);
    Mat4Data ToWire(const glm::mat4& value);
    glm::vec3 FromWire(const Vec3Data& value);
    glm::mat4 FromWire(const Mat4Data& value);
    Mat4Data MakePlayerTransform(const glm::vec3& position, float yaw, float pitch);
    bool TryReadPlayerTransform(const Mat4Data& value,
        glm::vec3& position, float& yaw, float& pitch);
}
