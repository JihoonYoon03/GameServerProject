#include "pch.h"
#include "Collision.h"

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
	const float length = glm::length(ray.direction);

	if (length < 0.000001f || sphere.radius <= 0.0f) return false;

	const glm::vec3 direction = ray.direction / length;
	const glm::vec3 offset = ray.origin - sphere.center;
	const float b = glm::dot(offset, direction);
	const float c = glm::dot(offset, offset) - sphere.radius * sphere.radius;
	const float discriminant = b * b - c;

	if (discriminant <= 0.0f) return false;

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
	const float length = glm::length(ray.direction);

	if (length < 0.000001f) return false;

	const glm::vec3 direction = ray.direction / length;
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
