#include "pch.h"
#include "ShootingScene.h"

Object* ShootingScene::AddProp(const std::string& name, const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Material>& material, const glm::vec3& position, const glm::vec3& scale, bool solid)
{
	auto object = std::make_unique<Object>(mesh, material);
	object->name = name;
	object->transform.position = position;
	object->transform.scale = scale;
	object->blocksMovement = solid;
	object->collider = solid ? ColliderType::Box : ColliderType::None;

	return AddObject(std::move(object));
}

void ShootingScene::Initialize()
{
	const auto shader = std::make_shared<Shader>("Shaders/Lit.vert", "Shaders/Lit.frag");
	const auto cube = assets_.LoadMesh("Resources/Meshes/Cube.obj");
	const auto sphere = assets_.LoadMesh("Resources/Meshes/Sphere.obj");

	auto sky = std::make_shared<Material>(shader);
	sky->texture = assets_.LoadTexture("Resources/Textures/SkydomeTex.png");
	sky->unlit = true;
	sky->doubleSided = true;
	auto* dome = AddProp("Sky", sphere, sky, glm::vec3(0), glm::vec3(150));
	dome->transform.rotation.y = -90.0f;

	auto ground = std::make_shared<Material>(shader);
	ground->texture = assets_.LoadTexture("Resources/Textures/GroundTex.png");
	ground->color = {0.55f, 0.37f, 0.17f};
	// 원본 draw.h는 바닥 텍스처를 활성화하지 않고 기본색으로 그렸습니다.
	ground->useTexture = false;
	// 원본 0 스케일 바닥의 환경광 외형을 유지하면서 특이 노멀 행렬은 제거합니다.
	ground->ambientOnly = true;

	std::vector<Vertex> floorVertices;

	for (auto vertex : cube->GetVertices())
	{
		if (std::abs(vertex.normal.y) > 0.5f)
		{
			vertex.position.y = 0.0f;
			floorVertices.push_back(vertex);
		}
	}

	auto* floor = AddProp("Ground", std::make_shared<Mesh>(std::move(floorVertices)), ground, {0, 0, 0}, {5, 1, 5});
	floor->collider = ColliderType::Box;

	auto fence = std::make_shared<Material>(shader);
	fence->color = glm::vec3(0.9f);
	AddProp("Shooting line", cube, fence, {0, 0.375f, -0.25f}, {5, 0.75f, 0.05f}, true);

	auto* right = AddProp("Right fence", cube, fence, {5, 0.375f, 0}, {5, 0.75f, 0.05f}, true);
	right->transform.rotation.y = 90.0f;

	auto* left = AddProp("Left fence", cube, fence, {-5, 0.375f, 0}, {5, 0.75f, 0.05f}, true);
	left->transform.rotation.y = 90.0f;
	AddProp("Back fence", cube, fence, {0, 0.375f, 5}, {5, 0.75f, 0.05f}, true);
	AddProp("Front fence", cube, fence, {0, 0.375f, -5}, {5, 0.75f, 0.05f}, true);

	auto playerMaterial = std::make_shared<Material>(shader);
	playerMaterial->color = {0.8f, 0.8f, 0.2f};

	auto player = std::make_unique<Player>(cube, playerMaterial);
	player_ = player.get();
	player->name = "Local player";
	player->transform.position = {0, 1, 1};
	player->transform.scale = glm::vec3(0.25f);
	player->collider = ColliderType::Box;
	AddObject(std::move(player));
	player_->UpdateCamera(camera_);

	auto gunMaterial = std::make_shared<Material>(shader);
	gunMaterial->color = glm::vec3(0.2f);

	auto gun = std::make_unique<Gun>(assets_.LoadMesh("Resources/Meshes/Pistol.obj"), gunMaterial);
	gun_ = gun.get();
	gun->name = "Pistol";
	gun->transform.scale = glm::vec3(0.025f);
	gun->FollowCamera(camera_, player_->transform.position);
	AddObject(std::move(gun));

	auto targetMaterial = std::make_shared<Material>(shader);
	targetMaterial->color = {1.0f, 0.1f, 0.1f};

	std::uniform_real_distribution<float> x(-5.0f, 5.0f), y(0.5f, 2.0f), z(-5.0f, -0.5f);

	for (int i = 0; i < 90; ++i)
	{
		auto target = std::make_unique<Target>(sphere, targetMaterial);
		target->name = "Target " + std::to_string(i + 1);
		target->transform.position = {x(random_), y(random_), z(random_)};
		target->transform.scale = glm::vec3(0.15f);
		target->collider = ColliderType::Sphere;
		target->active = false;
		targets_.push_back(target.get());
		AddObject(std::move(target));
	}

	lights_.push_back(Light::Point({100, 150, 110}, glm::vec3(1), 1.0f, 0.0f));
}

void ShootingScene::Update(float deltaTime, const Input& input)
{
	for (unsigned char key = '1'; key <= '3'; ++key)
	{
		if (input.WasPressed(key))
		{
			stages_.Start(key - '0');
		}
	}

	if (input.WasPressed('/'))
	{
		debugColliders = !debugColliders;
	}

	const StageEvents events = stages_.Update(deltaTime);

	for (int i = 0; i < events.spawnCount; ++i)
	{
		SpawnTarget();
	}

	if (events.clearTargets)
	{
		for (auto* target : targets_)
		{
			target->active = false;
		}
	}

	PlayerCommand command;
	command.movement.x = static_cast<float>(input.IsDown('d')) - static_cast<float>(input.IsDown('a'));
	command.movement.y = static_cast<float>(input.IsDown('s')) - static_cast<float>(input.IsDown('w'));
	command.lookDelta = input.mouseDelta;
	command.fire = input.firePressed;
	ApplyPlayerCommand(command, deltaTime);
	Shoot();
	Scene::Update(deltaTime, input);
	gun_->FollowCamera(camera_, player_->transform.position);
}

void ShootingScene::ApplyPlayerCommand(const PlayerCommand& command, float deltaTime)
{
	player_->ApplyCommand(command, deltaTime, *this);
	player_->UpdateCamera(camera_);

	if (command.fire)
	{
		gun_->Fire();
		shot_ = {camera_.position, camera_.GetForward()};
		hasShot_ = true;
	}
}

void ShootingScene::SpawnTarget()
{
	std::vector<Target*> available;

	for (auto* target : targets_)
	{
		if (!target->active)
		{
			available.push_back(target);
		}
	}

	if (available.empty()) return;

	std::uniform_int_distribution<size_t> pick(0, available.size() - 1);
	available[pick(random_)]->active = true;
}

void ShootingScene::Shoot()
{
	if (!hasShot_) return;

	// 원본은 마지막 광선을 유지하고 매 업데이트마다 등록 순서의 첫 표적을 검사합니다.

	for (auto* target : targets_)
	{
		float distance = 0.0f;

		if (target->active && Collision::Raycast(shot_, target->GetWorldSphere(), distance) && target->Hit())
		{
			stages_.AddScore();
			break;
		}
	}
}

Hud ShootingScene::GetHud() const
{
	Hud hud;
	hud.lines = {"stage: " + std::to_string(stages_.GetStageNumber()), "point: " + std::to_string(stages_.GetScore()),
		"target point: " + std::to_string(stages_.GetRequiredScore()),
		"timer: " + std::to_string(stages_.GetRemainingSeconds())};

	switch (stages_.GetState())
	{
	case StageState::Ready:
		hud.message = "stage" + std::to_string(stages_.GetStageNumber());
		hud.messageScale = 0.0035f;
		break;
	case StageState::GameOver:
		hud.message = "Game Over";
		break;
	case StageState::Clear:
		hud.message = "Game Clear!";
		break;
	default:
		break;
	}

	return hud;
}
