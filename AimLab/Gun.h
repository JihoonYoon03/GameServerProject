#pragma once
#include "pch.h"
#include "Object.h"
#include "Camera.h"

class Gun : public Object
{
public:
	using Object::Object;
	void Fire();
	void Update(float deltaTime) override;
	void FollowCamera(const Camera& camera);
	void FollowCamera(const Camera& camera, const glm::vec3& playerPosition);
	glm::mat4 GetModelMatrix() const override;

private:
	float	  recoilTime_ = 0.0f;
	float	  recoilAmount_ = 0.0f;
	bool	  recoiling_ = false;
	glm::mat4 cameraPivot_{1.0f};
};
