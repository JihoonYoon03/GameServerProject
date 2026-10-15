#pragma once
#include "pch.h"
#include "Scene.h"
#include "Player.h"
#include "Gun.h"
#include "Target.h"
#include "StageController.h"

class ShootingScene : public Scene
{
public:
	void Initialize() override;
	void Update(float deltaTime, const Input& input) override;
	Hud GetHud() const override;
	void ApplyPlayerCommand(const PlayerCommand& command, float deltaTime);

private:
	Object* AddProp(const std::string& name, const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Material>& material, const glm::vec3& position, const glm::vec3& scale, bool solid = false);
	void SpawnTarget();
	void Shoot();

	// 소유권은 Scene의 objects_에 있습니다. 아래 포인터는 현재 장면에서만 사용합니다.
	Player*				 player_ = nullptr;
	Gun*				 gun_ = nullptr;
	std::vector<Target*> targets_;
	StageController		 stages_;
	Ray					 shot_;
	bool				 hasShot_ = false;
	std::mt19937		 random_{std::random_device{}()};
};
