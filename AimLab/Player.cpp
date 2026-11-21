#include "pch.h"
#include "Player.h"
#include "Scene.h"
#include "../Shared/NetworkConversions.h"

namespace
{
	constexpr float HeadHeight = Net::PlayerHalfExtent * 2.0f;
}

void Player::ApplyCommand(const PlayerCommand& command, float deltaTime, const Scene& scene)
{
	m_fYaw = std::fmod(m_fYaw - command.lookDelta.x * m_fHorizontalSensitivity, 360.0f);
	m_fPitch = std::clamp(m_fPitch - command.lookDelta.y * m_fVerticalSensitivity, -89.0f, 89.0f);
	glm::vec2 movement = command.movement;

	if (glm::length(movement) > 1.0f)
	{
		movement = glm::normalize(movement);
	}

	glm::vec3 displacement =
		glm::vec3(GetBodyTransform() * glm::vec4(movement.x, 0, movement.y, 0)) * m_fMoveSpeed * deltaTime;

	//몸통 중심을 이동한 뒤 벽과 겹치면 이전 위치로 복구.
	glm::vec3 previousPosition = transform.position;
	transform.position += displacement;
	const Box bodyBox = GetWorldBox();
	for (const std::unique_ptr<Object>& object : scene.GetObjects())
	{
		if (object.get() != this && object->active && object->blocksMovement
			&& object->collider == ColliderType::Box && Collision::OverlapsXZ(bodyBox, object->GetWorldBox()))
		{
			transform.position = previousPosition;
			break;
		}
	}
}

void Player::UpdateCamera(Camera& camera) const
{
	camera.position = transform.position + glm::vec3(0.0f, HeadHeight, 0.0f);
	camera.yaw = m_fYaw;
	camera.pitch = m_fPitch;
}

glm::mat4 Player::GetBodyTransform() const
{
	//몸통 중심이 네트워크 위치이자 yaw 회전축.
	const glm::mat4 center = glm::translate(glm::mat4(1.0f), transform.position);
	return glm::rotate(center, glm::radians(m_fYaw), glm::vec3(0, 1, 0));
}

glm::mat4 Player::GetModelMatrix() const
{
	return glm::scale(GetBodyTransform(), transform.scale);
}

Box Player::GetWorldBox() const
{
	//서버와 동일하게 몸통 중심에서 고정 반폭으로 충돌 검사. 회전은 표시용에만 반영.
	const glm::vec3 half(Net::PlayerHalfExtent);
	return {transform.position - half, transform.position + half};
}

glm::mat4 Player::GetHeadTransform() const
{
	//몸통의 yaw를 따라간 뒤, 머리 중심에서 pitch 적용. 총기도 같은 축을 사용.
	const glm::mat4 headCenter = glm::translate(GetBodyTransform(), glm::vec3(0.0f, HeadHeight, 0.0f));
	return glm::rotate(headCenter, glm::radians(m_fPitch), glm::vec3(1, 0, 0));
}

Net::PlayerPose Player::GetNetworkPose() const
{
	return {transform.position, m_fYaw, m_fPitch};
}

bool Player::ApplyNetworkPose(const Net::PlayerPose& pose)
{
	if (!Net::IsFinitePlayerPose(pose))
		return false;

	transform.position = pose.position;
	m_fYaw = pose.yaw;
	m_fPitch = pose.pitch;
	return true;
}
