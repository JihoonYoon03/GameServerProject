#pragma once
#include "pch.h"

struct Box
{
	glm::vec3 min{0.0f};
	glm::vec3 max{0.0f};
};

struct Sphere
{
	glm::vec3 center{0.0f};
	float	  radius = 0.0f;
};

struct Ray
{
	glm::vec3 origin{0.0f};
	glm::vec3 direction{0.0f, 0.0f, -1.0f};
};

enum class ColliderType
{
	None,
	Box,
	Sphere
};

// 그래픽 API와 무관한 순수 계산입니다. 서버에서도 같은 판정을 사용할 수 있습니다.
namespace Collision
{
	bool Overlaps(const Box& a, const Box& b);
	// 원본 사격장의 이동 판정: 높이를 비교하지 않으며 경계 접촉도 충돌입니다.
	bool OverlapsXZ(const Box& a, const Box& b);
	bool Raycast(const Ray& ray, const Sphere& sphere, float& distance);
	bool Raycast(const Ray& ray, const Box& box, float& distance);
} // namespace Collision
