#pragma once
#include "pch.h"
#include "Object.h"
class Player;

class Gun : public Object
{
public:
	Gun(const Player& player, std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material);
	~Gun();

	void Update(float deltaTime) override;

	void ResetGame();
	bool Fire();
	bool TryFire();
	void StartReload();
	void UpdateReload(float deltaTime);
	uint8_t GetAmmo() const { return m_nAmmo; }
	bool IsReloading() const { return m_bIsReloading; }

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

	const Player& m_Player;
};
