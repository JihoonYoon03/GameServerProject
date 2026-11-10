#include "pch.h"
#include "Gun.h"

Gun::Gun(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material)
	: Object(mesh, material),
	m_fFireCooldown(0.0f),
	m_fReloadRemainingTime(0.0f),
	m_nAmmo(Net::MagazineCapacity),
	m_bIsReloading(false),
	m_fRecoilElapsedTime(0.0f),
	m_fRecoilProgress(0.0f),
	m_bIsRecoiling(false),
	m_mtx4CameraPivot(glm::mat4x4{})
{
}

Gun::~Gun()
{
}

void Gun::Update(float deltaTime)
{
	//발사 속도
	if (m_fFireCooldown > 0.0f)
	{
		m_fFireCooldown -= deltaTime;

		if (m_fFireCooldown < 0.0f)
			m_fFireCooldown = 0.0f;
	}

	//재장전
	if (m_fReloadRemainingTime > 0.0f)
	{
		m_fReloadRemainingTime -= deltaTime;

		if (m_fReloadRemainingTime < 0.0f)
		{
			m_fReloadRemainingTime = 0.0f;
			m_nAmmo = Net::MagazineCapacity;
		}
	}

	//아래는 반동 관련
	if (m_bIsRecoiling)
	{
		//반동 진행 시간에 따라 두 구간으로 나누고, 해당 구간의 몇%가 지나는지 recoilAmount에 저장
		m_fRecoilProgress = m_fRecoilElapsedTime < 0.05f ? m_fRecoilElapsedTime / 0.05f : (1.0f - m_fRecoilElapsedTime) / 0.95f;
		//반동 진행 시간 갱신.
		m_fRecoilElapsedTime = std::min(1.0f, m_fRecoilElapsedTime + deltaTime * Net::RecoilAnimSpeed);

		if (m_fRecoilElapsedTime >= 1.0f)
		{
			m_bIsRecoiling = false;
			m_fRecoilElapsedTime = 0.0f;
		}
	}
}

bool Gun::Fire()
{
	//현재는 Fire과 TryFire를 나눈 의미가 크게 없음. 추후에 기능 추가?
	return TryFire();
}

bool Gun::TryFire()
{
	if (m_fFireCooldown > 0.0f) return false;
	if (m_nAmmo == 0)
	{
		if (m_fReloadRemainingTime == 0.0f)
			m_fReloadRemainingTime = Net::ReloadSeconds;

		return false;
	}

	--m_nAmmo;
	m_fFireCooldown = Net::FireIntervalSeconds;

	m_bIsRecoiling = true;
	m_fRecoilElapsedTime = 0.0f;
	m_fRecoilProgress = 0.0f;

	return true;
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
	m_mtx4CameraPivot = glm::translate(glm::mat4(1), camera.position) * camera.GetRotation() *
				   glm::translate(glm::mat4(1), -camera.position);
}

glm::mat4 Gun::GetModelMatrix() const
{
	const glm::vec3 localPosition = glm::vec3(0.0f, 0.1f, -0.2f) * m_fRecoilProgress / transform.scale;
	const auto pitchRotation = glm::angleAxis(glm::radians(-15.0f * m_fRecoilProgress), glm::vec3(1, 0, 0));
	glm::mat4 animation = glm::mat4_cast(pitchRotation);
	animation = glm::translate(animation, localPosition);

	return m_mtx4CameraPivot * transform.GetMatrix() * animation;
}
