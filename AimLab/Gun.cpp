#include "pch.h"
#include "Gun.h"

void Gun::Fire()
{
	recoiling_ = true;
	recoilTime_ = 0.0f;
	recoilAmount_ = 0.0f;
}

void Gun::Update(float deltaTime)
{
	// Character.cpp의 키프레임을 먼저 평가한 뒤 시간을 증가시키는 순서를 보존합니다.
	recoilAmount_ = 0.0f;

	if (recoiling_)
	{
		recoilAmount_ = recoilTime_ < 0.05f ? recoilTime_ / 0.05f : (1.0f - recoilTime_) / 0.95f;
		recoilTime_ = std::min(1.0f, recoilTime_ + deltaTime * 1.5f);

		if (recoilTime_ >= 1.0f)
		{
			recoiling_ = false;
			recoilTime_ = 0.0f;
		}
	}
}

void Gun::FollowCamera(const Camera& camera)
{
	FollowCamera(camera, camera.position - glm::vec3(0.0f, 0.75f, -0.4f));
}

void Gun::FollowCamera(const Camera& camera, const glm::vec3& playerPosition)
{
	// 부모 이동과 카메라 중심 회전을 별도로 합성합니다. 충돌 직후에도 같은 값을 가정하지 않습니다.
	transform.position = playerPosition + glm::vec3(0.2f, 0.35f, -0.8f);
	transform.rotation = {0.0f, 180.0f, 0.0f};
	cameraPivot_ = glm::translate(glm::mat4(1), camera.position) * camera.GetRotation() *
				   glm::translate(glm::mat4(1), -camera.position);
}

glm::mat4 Gun::GetModelMatrix() const
{
	const glm::vec3 localPosition = glm::vec3(0.0f, 0.1f, -0.2f) * recoilAmount_ / transform.scale;
	const auto pitchRotation = glm::angleAxis(glm::radians(-15.0f * recoilAmount_), glm::vec3(1, 0, 0));
	glm::mat4 animation = glm::mat4_cast(pitchRotation);
	animation = glm::translate(animation, localPosition);

	return cameraPivot_ * transform.GetMatrix() * animation;
}
