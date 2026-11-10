#pragma once
#include "NetworkTypes.h"
#include <GL/glm/glm.hpp>
#include <cstddef>
#include <vector>

struct Transform
{
	glm::vec3 position{0.0f};
	glm::vec3 rotation{0.0f}; // 도 단위임: X(pitch), Y(yaw), Z(roll)
	glm::vec3 scale{1.0f};

	glm::mat4 GetMatrix() const;
};

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
	Box GetWorldBox(const Box& local, const Transform& transform);
	Sphere GetWorldSphere(const Box& local, const Transform& transform);

	bool Overlaps(const Box& a, const Box& b);
	// 원본 사격장의 이동 판정: 높이를 비교하지 않으며 경계 접촉도 충돌입니다.
	bool OverlapsXZ(const Box& a, const Box& b);

	bool Raycast(const Ray& ray, const Sphere& sphere, float& distance);
	bool Raycast(const Ray& ray, const Box& box, float& distance);

	// 표적 배열은 호출부의 표적 식별자 목록과 같은 순서이며, 가장 가까운 대상만 명중 처리한다.
	Net::ShotOutcome FindNearestHit(const Ray& ray,
		const std::vector<Box>& obstacles, const std::vector<Sphere>& targets,
		std::size_t& targetIndex, float& distance);
} // namespace Collision
