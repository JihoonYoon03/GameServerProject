#include "pch.h"
#include "Object.h"

glm::mat4 Transform::GetMatrix() const
{
	glm::mat4 matrix = glm::translate(glm::mat4(1.0f), position);
	matrix = glm::rotate(matrix, glm::radians(rotation.y), glm::vec3(0, 1, 0));
	matrix = glm::rotate(matrix, glm::radians(rotation.x), glm::vec3(1, 0, 0));
	matrix = glm::rotate(matrix, glm::radians(rotation.z), glm::vec3(0, 0, 1));

	return glm::scale(matrix, scale);
}

Object::Object(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material)
	: mesh_(std::move(mesh)), material_(std::move(material))
{
}

Box Object::GetWorldBox() const
{
	const Box local = mesh_ ? mesh_->GetBounds() : Box{glm::vec3(-0.5f), glm::vec3(0.5f)};
	const glm::mat4 matrix = transform.GetMatrix();
	Box world{glm::vec3(std::numeric_limits<float>::max()), glm::vec3(std::numeric_limits<float>::lowest())};
	// 회전된 메시도 여덟 꼭짓점으로 월드 AABB를 구합니다.

	for (int corner = 0; corner < 8; ++corner)
	{
		const glm::vec3 point((corner & 1) ? local.max.x : local.min.x, (corner & 2) ? local.max.y : local.min.y, (corner & 4) ? local.max.z : local.min.z);
		const glm::vec3 transformed = glm::vec3(matrix * glm::vec4(point, 1.0f));
		world.min = glm::min(world.min, transformed);
		world.max = glm::max(world.max, transformed);
	}

	return world;
}

Sphere Object::GetWorldSphere() const
{
	const Box local = mesh_ ? mesh_->GetBounds() : Box{glm::vec3(-0.5f), glm::vec3(0.5f)};
	const glm::vec3 size = (local.max - local.min) * glm::abs(transform.scale);
	const float radius = std::max({size.x, size.y, size.z}) * 0.5f;
	const auto center = glm::vec3(transform.GetMatrix() * glm::vec4((local.min + local.max) * 0.5f, 1.0f));

	return {center, radius};
}
