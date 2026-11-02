#pragma once
#include "pch.h"
#include "Scene.h"
#include "Player.h"
#include "Gun.h"
#include "Target.h"
#include "StageController.h"
#include "Shared/NetworkPackets.h"

class ShootingScene : public Scene
{
public:
	void Initialize() override;
	void Update(float deltaTime, const Input& input) override;
	Hud GetHud() const override;
	void ApplyPlayerCommand(const PlayerCommand& command, float deltaTime);

	// 멀티플레이 함수 선언. 실제 동작과 네트워크 연결은 이후 담당 일정에서 구현한다.
	void SendMapLoaded();
	PlayerCommand ReadPlayerInput(const Input& input) const;
	void UpdatePlayer(const PlayerCommand& command, float deltaTime);
	void UpdateCamera();
	void SendPlayerTransform();
	void RecvTransformData(const Net::PlayerSnapshot& snapshot);
	void RequestReadyChange(bool ready);
	void OnRoomStateChanged(const Net::RoomStateData& state);
	void OnGameStarted(const Net::GameStarted& event);
	void ShowReadyUI();
	void DrawHUD();
	void SendShoot();
	void RecvShootResult(const Net::ShotResolved& result);
	void ApplyTargetPositions(const Net::UdpHeader& header,
	    const Net::TargetSnapshot& snapshot);
	void ApplyTargetSnapshot(const std::vector<Net::TargetMotionData>& targets,
	    std::uint32_t serverTick, Net::ServerTimeMs serverTimeMs);
	void InterpolateTargets(Net::ServerTimeMs renderServerTimeMs);
	void UpdateMatchTime(const Net::MatchTime& time);
	void RecvGameResults(const Net::GameResults& results);
	void OnGameEnded();
	void ShowGameResult();

private:
	Object* AddProp(const std::string& name, const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Material>& material, const glm::vec3& position, const glm::vec3& scale, bool solid = false);
	void SpawnTargets();
	void RemoveDestroyedTargets();
	void ClearTargets();
	void Shoot();

	// 소유권은 Scene의 objects_에 있습니다. 아래 포인터는 현재 장면에서만 사용합니다.
	Player*				 player_ = nullptr;
	Gun*				 gun_ = nullptr;
	std::vector<Target*> targets_;
	std::shared_ptr<Mesh> targetMesh_;
	std::shared_ptr<Material> targetMaterial_;
	float targetSpawnTime_ = 0.0f;
	StageController		 stages_;
	Ray					 shot_;
	bool				 hasShot_ = false;
	std::mt19937		 random_{std::random_device{}()};
};
