#pragma once
#include "pch.h"
#include "Object.h"
#include "Camera.h"
#include "Shared/NetworkTypes.h"

class Scene;

// 로컬 입력을 게임 명령으로 바꾼 값입니다. 소켓 메시지를 해석한 뒤에도 이 경로를 사용합니다.
// 패킷으로 구조체 메모리를 그대로 전송하지 말고 필드별 직렬화와 서버 검증을 추가해야 합니다.
struct PlayerCommand
{
	glm::vec2 movement{0.0f};  // X: 좌우, Y: 전후 (전진은 -1)
	glm::vec2 lookDelta{0.0f}; // 마우스 이동량
	bool	  fire = false;
};

class Player : public Object
{
public:
	using Object::Object;

	void ApplyCommand(const PlayerCommand& command, float deltaTime, const Scene& scene);
	void UpdateCamera(Camera& camera) const;
	glm::mat4 GetModelMatrix() const override;

	Net::PlayerPose GetNetworkPose() const;
	bool ApplyNetworkPose(const Net::PlayerPose& pose);

	float	m_fYaw = 0.0f;
	float	m_fPitch = 0.0f;

	float	m_fMoveSpeed = Net::PlayerMoveSpeed;
	float	m_fSensitivity = 0.2f;

	// 화면 객체 ID와 네트워크 세션 토큰은 서로 다른 식별자이다.
	Net::SessionToken m_SessionToken{};

private:
	glm::vec3	m_vec3EyePosition{0.0f};
	bool		m_bEyeInitialized = false;

	glm::vec3	m_vec3LastDisplacement{0.0f};
};
