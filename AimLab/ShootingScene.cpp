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

	targetMesh_ = sphere;
	targetMaterial_ = std::make_shared<Material>(shader);
	targetMaterial_->color = {1.0f, 0.1f, 0.1f};

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

	const bool wasPlaying = stages_.GetState() == StageState::Playing;
	const StageEvents events = stages_.Update(deltaTime);

	if (events.clearTargets)
	{
		ClearTargets();
		targetSpawnTime_ = 0.0f;
	}

	PlayerCommand command;
	command.movement.x = static_cast<float>(input.IsDown('d')) - static_cast<float>(input.IsDown('a'));
	command.movement.y = static_cast<float>(input.IsDown('s')) - static_cast<float>(input.IsDown('w'));
	command.lookDelta = input.mouseDelta;
	command.fire = input.firePressed;
	ApplyPlayerCommand(command, deltaTime);
	// 기존 표적의 수명부터 갱신합니다. 이번 프레임에 생성할 표적은 아직 나이를 세지 않습니다.
	Scene::Update(deltaTime, input);
	RemoveDestroyedTargets();

	if (wasPlaying && stages_.GetState() == StageState::Playing)
	{
		targetSpawnTime_ += std::max(deltaTime, 0.0f);

		while (targetSpawnTime_ >= 0.5f)
		{
			targetSpawnTime_ -= 0.5f;
			SpawnTargets();
		}
	}
	else
	{
		targetSpawnTime_ = 0.0f;
	}

	Shoot();
	RemoveDestroyedTargets();
	gun_->FollowCamera(camera_, player_->transform.position);
}

void ShootingScene::ApplyPlayerCommand(const PlayerCommand& command, float deltaTime)
{
	player_->ApplyCommand(command, deltaTime, *this);
	player_->UpdateCamera(camera_);

	if (command.fire && gun_->Fire())
	{
		shot_ = {camera_.position, camera_.GetForward()};
		hasShot_ = true;
	}
}

void ShootingScene::SpawnTargets()
{
	std::uniform_real_distribution<float> x(-5.0f, 5.0f), y(0.5f, 2.0f), z(-5.0f, -0.5f);


	auto target = std::make_unique<Target>(targetMesh_, targetMaterial_);
	target->transform.position = {x(random_), y(random_), z(random_)};
	target->transform.scale = glm::vec3(0.15f);
	target->collider = ColliderType::Sphere;
	Target* spawned = target.get();
	AddObject(std::move(target));
	spawned->name = "Target " + std::to_string(spawned->GetId());
	targets_.push_back(spawned);

}

void ShootingScene::RemoveDestroyedTargets()
{
	// Object 순회가 끝난 뒤 관찰 포인터를 먼저 지우고 실제 소유 객체를 제거합니다.
	for (auto it = targets_.begin(); it != targets_.end();)
	{
		if ((*it)->IsPendingDestroy())
		{
			const uint32_t id = (*it)->GetId();
			it = targets_.erase(it);
			RemoveObject(id);
		}
		else
		{
			++it;
		}
	}
}

void ShootingScene::ClearTargets()
{
	for (auto* target : targets_)
	{
		RemoveObject(target->GetId());
	}

	targets_.clear();
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

void ShootingScene::ApplyTargetPositions(const Net::UdpHeader& header, const Net::TargetSnapshot& snapshot)
{
	// 헤더와 본문은 네트워크 바이트 순서에서 변환된 값으로 전달받습니다.
	if (header.magic != Net::UdpMagic || header.type != Net::UdpType::TargetSnapshot || header.context.roomId == 0) return;

	if (snapshot.totalTargetCount > Net::MaxTargets || snapshot.targetCount > snapshot.targets.size()) return;

	const std::size_t expectedChunkCount = std::max<std::size_t>(1,
		(snapshot.totalTargetCount + Net::MaxTargetsPerSnapshot - 1) / Net::MaxTargetsPerSnapshot);

	if (snapshot.chunkCount != expectedChunkCount || snapshot.chunkIndex >= snapshot.chunkCount) return;

	const std::size_t offset = snapshot.chunkIndex * Net::MaxTargetsPerSnapshot;
	const std::size_t expectedTargetCount = std::min<std::size_t>(Net::MaxTargetsPerSnapshot, snapshot.totalTargetCount - offset);

	if (snapshot.targetCount != expectedTargetCount) return;

	// SendTargetPositions에서 기록하는 필드 크기를 사용합니다.
	constexpr std::size_t HeaderBytes = 2 + 2 + 2 + Net::SessionTokenBytes + 4 + 4 + 4 + 4 + 4;
	constexpr std::size_t SnapshotMetadataBytes = 8 + 2 + 2 + 2 + 2;
	constexpr std::size_t TargetBytes = 4 + 3 * 4 + 3 * 4;
	const std::size_t expectedSize = HeaderBytes + SnapshotMetadataBytes + snapshot.targetCount * TargetBytes;

	if (header.size != expectedSize || header.size > Net::MaxUdpPacketBytes) return;

	for (std::size_t i = 0; i < snapshot.targetCount; ++i)
	{
		const auto& target = snapshot.targets[i];

		if (target.targetId == 0 ||
			!std::isfinite(target.position.x) || !std::isfinite(target.position.y) || !std::isfinite(target.position.z) ||
			!std::isfinite(target.velocity.x) || !std::isfinite(target.velocity.y) || !std::isfinite(target.velocity.z)) return;
	}

	// TODO: 현재 방과 참가자 정보를 연결하면 context, sessionToken, membershipId를 확인합니다.
	// TODO: context, sequence, serverTick, serverTimeMs가 같은 조각을 chunkIndex 순서로 조립합니다.
	// TODO: 모든 조각이 모이면 전체 목록을 ApplyTargetSnapshot에 전달합니다. 빈 목록도 반영합니다.
	// ApplyTargetSnapshot(assembledTargets, header.serverTick, snapshot.serverTimeMs);
}
