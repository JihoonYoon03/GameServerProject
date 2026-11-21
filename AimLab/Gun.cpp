#include "pch.h"
#include "Gun.h"
#include "Player.h"

Gun::Gun(const Player& player, std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material)
	: Object(mesh, material),
	m_fFireCooldown(0.0f),
	m_fReloadRemainingTime(0.0f),
	m_nAmmo(Net::MagazineCapacity),
	m_bIsReloading(false),
	m_fRecoilElapsedTime(0.0f),
	m_fRecoilProgress(0.0f),
	m_bIsRecoiling(false),
	m_Player(player)
{
	//머리 중심을 기준으로 한 총기 위치. 내 총기와 상대 총기에 동일하게 적용.
	transform.position = {0.2f, -0.4f, -0.4f};
	transform.rotation = {0.0f, 180.0f, 0.0f};
	transform.scale = glm::vec3(0.025f);
}

Gun::~Gun()
{
}

void Gun::Update(float deltaTime)
{
	deltaTime = std::max(deltaTime, 0.0f);
	//발사 속도
	if (m_fFireCooldown > 0.0f)
	{
		m_fFireCooldown -= deltaTime;

		if (m_fFireCooldown < 0.0f)
			m_fFireCooldown = 0.0f;
	}

	//재장전
	UpdateReload(deltaTime);

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

void Gun::ResetGame()
{
	// 이전 경기의 탄약, 재장전, 발사 대기 시간과 반동을 초기화한다.
	m_fFireCooldown = 0.0f;
	m_fReloadRemainingTime = 0.0f;
	m_nAmmo = Net::MagazineCapacity;
	m_bIsReloading = false;
	m_fRecoilElapsedTime = 0.0f;
	m_fRecoilProgress = 0.0f;
	m_bIsRecoiling = false;
}

bool Gun::Fire()
{
	//현재는 Fire과 TryFire를 나눈 의미가 크게 없음. 추후에 기능 추가?
	return TryFire();
}

bool Gun::TryFire()
{
	//발사 간격이 남았거나 재장전 중이면 탄 수와 반동 상태를 건드리지 않고 종료.
	if (m_fFireCooldown > 0.0f || m_bIsReloading) return false;
	if (m_nAmmo == 0)
	{
		//빈 탄창에서 발사를 시도하면 재장전 시작. 이번 발사는 처리하지 않음.
		StartReload();

		return false;
	}

	--m_nAmmo;
	m_fFireCooldown = Net::FireIntervalSeconds;

	m_bIsRecoiling = true;
	m_fRecoilElapsedTime = 0.0f;
	m_fRecoilProgress = 0.0f;

	return true;
}

void Gun::StartReload()
{
	//재장전 요청이 반복돼도 남은 시간을 처음부터 다시 세지 않게 함. 탄창이 가득 찬 경우도 제외.
	if (m_bIsReloading || m_nAmmo == Net::MagazineCapacity)
		return;
	m_bIsReloading = true;
	m_fReloadRemainingTime = Net::ReloadSeconds;
}

void Gun::UpdateReload(float deltaTime)
{
	if (!m_bIsReloading)
		return;
	//시간을 0 아래로 내리지 않음. deltaTime이 음수여도 재장전 시간이 다시 늘어나지 않게 제한.
	m_fReloadRemainingTime = std::max(0.0f, m_fReloadRemainingTime - std::max(deltaTime, 0.0f));
	//시간이 정확히 0이 된 경우도 완료 처리. 탄창을 채우고 재장전 상태를 같이 해제.
	if (m_fReloadRemainingTime == 0.0f)
	{
		m_nAmmo = Net::MagazineCapacity;
		m_bIsReloading = false;
	}
}

glm::mat4 Gun::GetModelMatrix() const
{
	glm::vec3 localPosition = glm::vec3(0.0f, 0.1f, -0.2f) * m_fRecoilProgress / transform.scale;
	glm::quat pitchRotation = glm::angleAxis(glm::radians(-15.0f * m_fRecoilProgress), glm::vec3(1, 0, 0));
	glm::mat4 animation = glm::mat4_cast(pitchRotation);
	animation = glm::translate(animation, localPosition);

	return m_Player.GetHeadTransform() * transform.GetMatrix() * animation;
}
