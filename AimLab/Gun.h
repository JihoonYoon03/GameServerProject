#pragma once
#include "pch.h"
#include "Object.h"
#include "Camera.h"

class Gun : public Object
{
public:
	Gun(std::shared_ptr<Mesh> mesh = nullptr, std::shared_ptr<Material> material = nullptr);
	~Gun();

	void Update(float deltaTime) override;

	bool Fire();
	bool TryFire();
	void StartReload();
	void UpdateReload(float deltaTime);

	void FollowCamera(const Camera& camera);
	void FollowCamera(const Camera& camera, const glm::vec3& playerPosition);
	glm::mat4 GetModelMatrix() const override;
	void UpdateRecoil(float deltaTime);

private:
	//사격 간격. 0일 때에만 사격. 매 사격 시 0.2로 초기화 및 업데이트마다 deltatime만큼 줄어듬.
	float		m_fFireCooldown;
	//장전 시간. 장전 시 2로 초기화, 업데이트마다 deltatime만큼 줄어듬.
	float		m_fReloadRemainingTime;

	//남은 총알 개수
	uint8_t		m_nAmmo;
	bool		m_bIsReloading;

	float		m_fRecoilElapsedTime;
	float		m_fRecoilProgress;
	bool		m_bIsRecoiling;

	glm::mat4	m_mtx4CameraPivot;
};
