#pragma once
#include "pch.h"
#include "Scene.h"
#include "Player.h"
#include "Gun.h"
#include "Target.h"
#include "StageController.h"
#include "Shared/NetworkPackets.h"

class NetworkClient;

class ShootingScene : public Scene
{
public:
	void Initialize() override;
	void Update(float deltaTime, const Input& input) override;
	Hud GetHud() const override;
	void ApplyPlayerCommand(const PlayerCommand& command, float deltaTime);

	// 세션 제어를 연결하고, 이동·표적·화면 함수는 각 담당 일정에서 구현한다.
	bool SetNetworkSession(NetworkClient& network, const Net::RoomEnterResult& result);
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
	void ApplyTargetSnapshot(const Net::TargetSnapshot& snapshot);
	void UpdateMatchTime(const Net::MatchTime& time);
	void RecvGameResults(const Net::GameResults& results);
	void OnGameEnded();
	void ShowGameResult();

private:
	// 통신 참조와 세션 상태는 장면이 살아 있는 동안 메인 스레드에서만 사용한다.
	NetworkClient* network_ = nullptr;
	Net::SessionToken sessionToken_{};
	Net::MembershipId membershipId_ = 0;
	Net::RoomStateData roomState_{};
	bool mapLoadedSent_ = false;
	Net::MatchId startedMatchId_ = 0;
	Net::ServerTimeMs serverTimeMs_ = 0;
	Net::ServerTimeMs startsAtMs_ = 0;
	Net::ServerTimeMs endsAtMs_ = 0;

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
