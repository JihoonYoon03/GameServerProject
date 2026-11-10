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
		glm::vec3 areaMin = area.movementBounds.min;
		glm::vec3 areaMax = area.movementBounds.max;
		float start = position[axis];
		float limit = std::clamp(start + movement,
			areaMin[axis] + half, areaMax[axis] - half);
		movement = limit - start;

		int crossAxis = axis == 0 ? 2 : 0;
		for (const Net::BoxData& obstacle : obstacles)
		{
			glm::vec3 min = obstacle.min;
			glm::vec3 max = obstacle.max;
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

	// The request supplies a goal pose. Movement starts from the server's stored position.
	if (!Net::IsFinitePlayerPose(input.pose))
		return false;

	glm::vec3 position = m_PlayerData.pose.position;
	//충돌 범위 설정. min은 영역의 최소값 + 플레이어 중심에서 충돌박스 가장자리 까지 거리의 절반.
	//max는 영역 최댓값 - ''
	glm::vec3 min = area.movementBounds.min + glm::vec3(Net::PlayerHalfExtent);
	glm::vec3 max = area.movementBounds.max - glm::vec3(Net::PlayerHalfExtent);
	//현재 서버 위치가 영역을 벗어난다고 판단되는 경우 종료
	if (!(position.x >= min.x && position.x <= max.x && position.z >= min.z && position.z <= max.z))
		return false;

	//서버 시각과 속도로 이동량 계산
	Net::ServerTimeMs elapsedMs = m_bHasProcessedInput
		//요청 받은 시각과 마지막으로 움직인 시각의 차이를 구함.
		//이 수치가 이동에 소요 가능한 최대 대기 시간보다 많으면 min으로 자름.
		//최대 대기 시간 제한이 없으면, 가만히 있다가 움직이는 경우
		//한 번에 많은 거리를 이동하게 되어버림.
		? (std::min)(receivedAtMs - m_nLastMovementTimeMs, MaxMovementBudgetMs)
		//첫 요청은 lastMovement 값이 없으므로 고정값을 줌
		: 1000 / Net::ServerTickRate;

	double maxDistance = Net::PlayerMoveSpeed * (elapsedMs / 1000.0);

	//현재 서버 위치에서 목표까지의 차이 x, z
	double dx = static_cast<double>(input.pose.position.x) - position.x;
	double dz = static_cast<double>(input.pose.position.z) - position.z;
	//hypot: 빗변 길이 구하기
	double distance = std::hypot(dx, dz);

	//이동 속도를 적용한 최대 이동 거리보다 더 멀리 이동함을 방지.
	double fraction = distance > maxDistance ? maxDistance / distance : 1.0;

	//이동 후 충돌에 대한 보정 수행
	position.x += ClipAxis(position, static_cast<float>(dx * fraction), 0, area, obstacles);
	position.z += ClipAxis(position, static_cast<float>(dz * fraction), 2, area, obstacles);

	//최종적으로 플레이어 위치 갱신
	m_PlayerData.pose = {position, std::fmod(input.pose.yaw, 360.0f), std::clamp(input.pose.pitch, -89.0f, 89.0f)};

	m_nLastProcessedInput = input.inputSequence;
	m_nLastMovementTimeMs = receivedAtMs;
	m_bHasProcessedInput = true;
	return true;
}
