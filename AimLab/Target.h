#pragma once
#include "pch.h"
#include "Object.h"

class Target : public Object
{
public:
	Target(std::shared_ptr<Mesh> mesh = nullptr, std::shared_ptr<Material> material = nullptr);

	void Update(float deltaTime) override;

	bool IsPendingDestroy() const { return pendingDestroy_; }

	bool Hit();

private:
	glm::vec3 moveDirection_;
	// 삭제 여부는 Target이 결정하고, 실제 해제는 Scene의 순회가 끝난 뒤 처리합니다.
	float elapsedTime_ = 0.0f;
	float lifetime_ = 3.0f;
	bool pendingDestroy_ = false;
};
