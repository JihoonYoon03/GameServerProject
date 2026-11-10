#include "pch.h"
#include "Player.h"
#include "Scene.h"

void Player::ApplyCommand(const PlayerCommand& command, float deltaTime, const Scene& scene)
{
	m_fYaw = std::fmod(m_fYaw - command.lookDelta.x * m_fSensitivity, 360.0f);
	m_fPitch = std::clamp(m_fPitch - command.lookDelta.y * m_fSensitivity, -89.0f, 89.0f);
	glm::vec2 movement = command.movement;

	if (glm::length(movement) > 1.0f)
	{
		movement = glm::normalize(movement);
	}

	const auto rotation = glm::mat4_cast(glm::angleAxis(glm::radians(m_fYaw), glm::vec3(0, 1, 0)));
	const glm::vec3 displacement =
		glm::vec3(rotation * glm::vec4(movement.x, 0, movement.y, 0)) * m_fMoveSpeed * deltaTime;
	// 원본은 X/Z 중심 판정 후 이번 이동 전체를 취소합니다.

	if (movement != glm::vec2(0))
	{
		transform.position += displacement;
		m_vec3LastDisplacement = displacement;
		m_vec3EyePosition = transform.position + glm::vec3(0.0f, 0.75f, -0.4f);
		m_bEyeInitialized = true;
	}

	for (const auto& object : scene.GetObjects())
	{
		if (object.get() != this && object->active && object->blocksMovement && object->collider == ColliderType::Box && Collision::OverlapsXZ(GetWorldBox(), object->GetWorldBox()))
		{
			transform.position -= m_vec3LastDisplacement;
		}
	}
}

void Player::UpdateCamera(Camera& camera) const
{
	// 원본은 이동 후 카메라를 갱신하고 충돌을 처리했습니다. 그 관찰 순서를 유지합니다.
	camera.position = m_bEyeInitialized ? m_vec3EyePosition : transform.position + glm::vec3(0.0f, 0.75f, -0.4f);
	camera.yaw = m_fYaw;
	camera.pitch = m_fPitch;
}

glm::mat4 Player::GetModelMatrix() const
{
	const glm::vec3 eye = m_bEyeInitialized ? m_vec3EyePosition : transform.position + glm::vec3(0.0f, 0.75f, -0.4f);
	glm::mat4 pivot = glm::translate(glm::mat4(1), eye);
	pivot *= glm::mat4_cast(glm::angleAxis(glm::radians(m_fYaw), glm::vec3(0, 1, 0)));

	return glm::translate(pivot, -eye) * transform.GetMatrix();
}
