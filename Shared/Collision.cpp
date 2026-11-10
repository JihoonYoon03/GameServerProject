//서버 클라 공유 소스코드임
#include "Collision.h"
#include <GL/glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

glm::mat4 Transform::GetMatrix() const
{
	glm::mat4 matrix = glm::translate(glm::mat4(1.0f), position);
	matrix = glm::rotate(matrix, glm::radians(rotation.x), glm::vec3(1, 0, 0));
	matrix = glm::rotate(matrix, glm::radians(rotation.y), glm::vec3(0, 1, 0));
	matrix = glm::rotate(matrix, glm::radians(rotation.z), glm::vec3(0, 0, 1));

	return glm::scale(matrix, scale);
}

Box Collision::GetWorldBox(const Box& local, const Transform& transform)
{
	glm::mat4 matrix = transform.GetMatrix();
	Box world{glm::vec3(std::numeric_limits<float>::max()), glm::vec3(std::numeric_limits<float>::lowest())};
	
	for (int corner = 0; corner < 8; ++corner)
	{
		glm::vec3 point((corner & 1) ? local.max.x : local.min.x, (corner & 2) ? local.max.y : local.min.y, (corner & 4) ? local.max.z : local.min.z);
		glm::vec3 transformed = glm::vec3(matrix * glm::vec4(point, 1.0f));
		world.min = glm::min(world.min, transformed);
		world.max = glm::max(world.max, transformed);
	}

	return world;
}

Sphere Collision::GetWorldSphere(const Box& local, const Transform& transform)
{
	glm::vec3 size = (local.max - local.min) * glm::abs(transform.scale);
	float radius = std::max({size.x, size.y, size.z}) * 0.5f;
	glm::vec3 center = glm::vec3(transform.GetMatrix() * glm::vec4((local.min + local.max) * 0.5f, 1.0f));

	return {center, radius};
}

bool Collision::Overlaps(const Box& a, const Box& b)
{
	return a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y && a.max.y > b.min.y && a.min.z < b.max.z &&
		   a.max.z > b.min.z;
}

bool Collision::OverlapsXZ(const Box& a, const Box& b)
{
	return a.min.x <= b.max.x && a.max.x >= b.min.x && a.min.z <= b.max.z && a.max.z >= b.min.z;
}

bool Collision::Raycast(const Ray& ray, const Sphere& sphere, float& distance)
{
	float length = glm::length(ray.direction);

	//0벡터인 경우 or 구체 반지름이 없음
	if (length < 0.000001f || sphere.radius <= 0.0f) return false;

	//자세한 계산식은 게임수학 강의자료 참고
	glm::vec3 direction = ray.direction / length;
	glm::vec3 offset = ray.origin - sphere.center;
	float b = glm::dot(offset, direction);
	float c = glm::dot(offset, offset) - sphere.radius * sphere.radius;
	float discriminant = b * b - c;

	//판별식 결과 0 이하 = 교차 X 혹은 표면 한 점에서 스침
	if (discriminant <= 0.0f) return false;

	//실제 값 계산(결과는 양수만)
	float hit = -b - std::sqrt(discriminant);

	if (hit < 0.0f)
	{
		hit = -b + std::sqrt(discriminant);
	}

	if (hit < 0.0f) return false;

	distance = hit;

	return true;
}

bool Collision::Raycast(const Ray& ray, const Box& box, float& distance)
{
	float length = glm::length(ray.direction);

	if (length < 0.000001f) return false;

	glm::vec3 direction = ray.direction / length;
	float nearDistance = 0.0f;
	float farDistance = std::numeric_limits<float>::max();

	for (int axis = 0; axis < 3; ++axis)
	{
		if (std::abs(direction[axis]) < 0.000001f)
		{
			if (ray.origin[axis] < box.min[axis] || ray.origin[axis] > box.max[axis]) return false;

			continue;
		}

		float a = (box.min[axis] - ray.origin[axis]) / direction[axis];
		float b = (box.max[axis] - ray.origin[axis]) / direction[axis];

		if (a > b)
		{
			std::swap(a, b);
		}

		nearDistance = std::max(nearDistance, a);
		farDistance = std::min(farDistance, b);

		if (nearDistance > farDistance) return false;
	}

	distance = nearDistance;

	return true;
}
