#include "pch.h"
#include "ClientPlayer.h"
#include "../Shared/NetworkConversions.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr Net::ServerTimeMs MaxMovementBudgetMs = 100;

	float ClipAxis(const glm::vec3& position, float movement, int axis,
		const Net::AreaData& area, const std::vector<Net::BoxData>& obstacles)
	{
		float half = Net::PlayerHalfExtent;
		auto areaMin = Net::FromWire(area.movementBounds.min);
		auto areaMax = Net::FromWire(area.movementBounds.max);
		float start = position[axis];
		float limit = std::clamp(start + movement,
			areaMin[axis] + half, areaMax[axis] - half);
		movement = limit - start;

		const int crossAxis = axis == 0 ? 2 : 0;
		for (const auto& obstacle : obstacles)
		{
			auto min = Net::FromWire(obstacle.min);
			auto max = Net::FromWire(obstacle.max);
			if (position[crossAxis] + half <= min[crossAxis] ||
				position[crossAxis] - half >= max[crossAxis])
				continue;

			// Stop at the first wall along this axis, even when the goal is beyond it.
			if (movement > 0.0f && start <= min[axis] - half)
				movement = (std::min)(movement, (min[axis] - half) - start);
			else if (movement < 0.0f && start >= max[axis] + half)
				movement = (std::max)(movement, (max[axis] + half) - start);
		}
		return movement;
	}
}

bool ClientPlayer::ApplyMovement(const Net::PlayerTransform& input,
	const Net::AreaData& area, const std::vector<Net::BoxData>& obstacles,
	Net::ServerTimeMs receivedAtMs)
{
	//맵 로딩 X or 구역 배정받지 않음 or 구역 다름 or 첫 입력처리 완료했고 새 이동 요청이 아닌 경우
	//or 서버가 요청받은 시각이 역행함(lastMovement~ = 마지막으로 수락한 요청의 서버 수신 시각)
	if (m_PlayerData.mapLoaded == 0 || m_PlayerData.areaId == Net::UnassignedArea || area.areaId != m_PlayerData.areaId ||
		(m_bHasProcessedInput && (!Net::IsNewerSequence(input.inputSequence, m_nLastProcessedInput) ||
			receivedAtMs < m_nLastMovementTimeMs)))
		return false;

	glm::vec3 requestedPosition;
	float yaw = 0.0f;
	float pitch = 0.0f;
	if (!Net::TryReadPlayerTransform(input.worldTransform, requestedPosition, yaw, pitch))
		return false;

	glm::vec3 position = glm::vec3(Net::FromWire(m_PlayerData.worldTransform)[3]);
	auto min = Net::FromWire(area.movementBounds.min) + glm::vec3(Net::PlayerHalfExtent);
	auto max = Net::FromWire(area.movementBounds.max) - glm::vec3(Net::PlayerHalfExtent);
	if (!(position.x >= min.x && position.x <= max.x && position.z >= min.z && position.z <= max.z))
		return false;

	//서버 시각과 속도로 이동량 계산
	//첫 요청은 lastMovement 값이 없으므로 고정값을 줌
	auto elapsedMs = m_bHasProcessedInput
		? (std::min)(receivedAtMs - m_nLastMovementTimeMs, MaxMovementBudgetMs)
		: 1000 / Net::ServerTickRate;

	double maxDistance = Net::PlayerMoveSpeed * (elapsedMs / 1000.0);

	//현재 서버 위치에서 목표까지의 차이 x, z
	double dx = static_cast<double>(requestedPosition.x) - position.x;
	double dz = static_cast<double>(requestedPosition.z) - position.z;
	double distance = std::hypot(dx, dz);

	double fraction = distance > maxDistance ? maxDistance / distance : 1.0;

	//이동 후 플레이어 중심 기준 갈 수 있는 범위 구하기
	position.x += ClipAxis(position, static_cast<float>(dx * fraction), 0, area, obstacles);
	position.z += ClipAxis(position, static_cast<float>(dz * fraction), 2, area, obstacles);

	m_PlayerData.worldTransform = Net::MakePlayerTransform(position, yaw, std::clamp(pitch, -89.0f, 89.0f));

	m_mtx4LastProcessedTransform = m_PlayerData.worldTransform;
	m_nLastProcessedInput = input.inputSequence;
	m_nLastMovementTimeMs = receivedAtMs;
	m_bHasProcessedInput = true;
	return true;
}
