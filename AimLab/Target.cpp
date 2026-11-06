#include "pch.h"
#include "Target.h"

Target::Target(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material)
	: Object(std::move(mesh), std::move(material))
{
	static std::mt19937 random{std::random_device{}()};
	std::uniform_int_distribution<int> horizontal(-1, 1);
	moveDirection_ = glm::normalize(glm::vec3(static_cast<float>(horizontal(random)), 1.0f, 0.0f));
}

void Target::Update(float deltaTime)
{
	if (pendingDestroy_ || deltaTime <= 0.0f) return;

	constexpr float MoveSpeed = 1.0f;
	const float moveTime = std::min(deltaTime, lifetime_ - elapsedTime_);
	transform.position += moveDirection_ * (MoveSpeed * moveTime);
	elapsedTime_ += deltaTime;

	if (elapsedTime_ >= lifetime_)
	{
		pendingDestroy_ = true;
	}
}

bool Target::Hit()
{
	if (!active || pendingDestroy_) return false;

	pendingDestroy_ = true;
	return true;
}