#include "pch.h"
#include "ShootingScene.h"
#include "NetworkClient.h"
#include "../Shared/PacketCodec.h"
#include "../Shared/ShootingRange.h"

namespace
{
	constexpr float TargetAreaHalfSize = ShootingRange::TargetHalfSize;
	constexpr float TargetScale = ShootingRange::TargetRadius;
	constexpr Net::ServerTimeMs HitIndicatorDurationMs = 1000;

	class PlayerHead : public Object
	{
	public:
		PlayerHead(const Player& player, const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Material>& material)
			: Object(mesh, material), player_(player) {}

		glm::mat4 GetModelMatrix() const override
		{
			return glm::scale(player_.GetHeadTransform(), player_.transform.scale);
		}

	private:
		const Player& player_;
	};
}

Object* ShootingScene::AddProp(const std::string& name, const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Material>& material, const glm::vec3& position, const glm::vec3& scale, bool solid)
{
	std::unique_ptr<Object> object = std::make_unique<Object>(mesh, material);
	object->name = name;
	object->transform.position = position;
	object->transform.scale = scale;
	object->blocksMovement = solid;
	object->collider = solid ? ColliderType::Box : ColliderType::None;

	return AddObject(std::move(object));
}

ShootingScene::PlayerModel ShootingScene::AddPlayer(const Net::SessionToken& token,
	const std::shared_ptr<Material>& bodyMaterial, bool local)
{
	const auto cube = assets_.LoadMesh("Resources/Meshes/Cube.obj");
	auto body = std::make_unique<Player>(cube, bodyMaterial);
	body->m_SessionToken = token;
	body->name = local ? "Local player" : "Remote player";
	body->transform.scale = glm::vec3(Net::PlayerHalfExtent);
	body->collider = ColliderType::Box;
	auto head = std::make_unique<PlayerHead>(*body, cube, bodyMaterial);
	head->name = "Player head";
	//1인칭 카메라에서 자기 몸통과 머리가 시야를 막지 않도록 함.
	body->visible = head->visible = !local;

	auto gun = std::make_unique<Gun>(*body, assets_.LoadMesh("Resources/Meshes/Revolver.obj"), gunMaterial_);
	gun->name = "Revolver";

	PlayerModel model{body.get(), head.get(), gun.get()};
	AddObject(std::move(body));
	AddObject(std::move(head));
	AddObject(std::move(gun));
	return model;
}

void ShootingScene::RemovePlayerModel(const PlayerModel& model)
{
	RemoveObject(model.gun->GetId());
	RemoveObject(model.head->GetId());
	RemoveObject(model.body->GetId());
}

void ShootingScene::Initialize()
{
	std::shared_ptr<Shader> shader = std::make_shared<Shader>("Shaders/Lit.vert", "Shaders/Lit.frag");
	std::shared_ptr<Mesh> cube = assets_.LoadMesh("Resources/Meshes/Cube.obj");
	std::shared_ptr<Mesh> sphere = assets_.LoadMesh("Resources/Meshes/Sphere.obj");

	std::shared_ptr<Material> sky = std::make_shared<Material>(shader);
	sky->texture = assets_.LoadTexture("Resources/Textures/SkydomeTex.png");
	sky->unlit = true;
	sky->doubleSided = true;
	Object* dome = AddProp("Sky", sphere, sky, glm::vec3(0), glm::vec3(150));
	dome->transform.rotation.y = -90.0f;

	std::shared_ptr<Material> ground = std::make_shared<Material>(shader);
	ground->texture = assets_.LoadTexture("Resources/Textures/GroundTex.png");
	ground->color = {0.55f, 0.37f, 0.17f};
	// 원본 draw.h는 바닥 텍스처를 활성화하지 않고 기본색으로 그렸습니다.
	ground->useTexture = false;
	// 원본 0 스케일 바닥의 환경광 외형을 유지하면서 특이 노멀 행렬은 제거합니다.
	ground->ambientOnly = true;

	std::vector<Vertex> floorVertices;

	for (Vertex vertex : cube->GetVertices())
	{
		if (std::abs(vertex.normal.y) > 0.5f)
		{
			vertex.position.y = 0.0f;
			floorVertices.push_back(vertex);
		}
	}

	AddProp("Ground", std::make_shared<Mesh>(std::move(floorVertices)), ground, {0, 0, 0}, {12, 1, 12});

	std::shared_ptr<Material> targetAreaMaterial = std::make_shared<Material>(shader);
	targetAreaMaterial->color = {0.12f, 0.55f, 0.45f};
	constexpr float TargetPlateHalfThickness = 0.04f;
	AddProp("Target spawn plate", cube, targetAreaMaterial, {0, TargetPlateHalfThickness, 0},
		{TargetAreaHalfSize, TargetPlateHalfThickness, TargetAreaHalfSize});

	std::shared_ptr<Material> areaMaterial = std::make_shared<Material>(shader);
	areaMaterial->color = {0.68f, 0.78f, 0.88f};


	std::vector<Vertex> areaVertices;
	areaVertices.reserve(cube->GetVertices().size() * ShootingRange::AreaParts.size());
	for (const ShootingRange::AreaPart& part : ShootingRange::AreaParts)
	{
		for (Vertex vertex : cube->GetVertices())
		{
			vertex.position = part.offset + vertex.position * part.halfSize;
			areaVertices.push_back(vertex);
		}
	}
	std::shared_ptr<Mesh> areaMesh = std::make_shared<Mesh>(std::move(areaVertices));

	const glm::vec3 southArea = ShootingRange::AreaCenters[ShootingRange::SouthArea];
	const std::array<const char*, 4> areaNames{{"East player area", "West player area", "North player area", "South player area"}};
	for (std::size_t index = 0; index < ShootingRange::AreaCenters.size(); ++index)
	{
		const char* name = areaNames[index];
		glm::vec3 center = ShootingRange::AreaCenters[index];
		AddProp(name, areaMesh, areaMaterial, center, glm::vec3(1.0f));
		for (const ShootingRange::AreaPart& part : ShootingRange::AreaParts)
		{
			if (part.solid)
			{
				Object* wall = AddProp(name, cube, areaMaterial, center + part.offset, part.halfSize, true);
				wall->visible = false;
			}
		}
	}

	std::shared_ptr<Material> playerMaterial = std::make_shared<Material>(shader);
	playerMaterial->color = {0.8f, 0.8f, 0.2f};
	if (network_ != nullptr)
	{
		remotePlayerMaterial_ = std::make_shared<Material>(shader);
		remotePlayerMaterial_->color = {0.2f, 0.65f, 1.0f};
	}

	gunMaterial_ = std::make_shared<Material>(shader);
	gunMaterial_->color = glm::vec3(0.2f);
	const PlayerModel local = AddPlayer(sessionToken_, playerMaterial, true);
	player_ = local.body;
	gun_ = local.gun;
	player_->transform.position = southArea + glm::vec3(0, 1, 0);
	for (const Net::PlayerData& entry : roomState_.vPlayers)
		if (entry.m_sessionToken == sessionToken_)
			player_->ApplyNetworkPose(entry.pose);
	player_->UpdateCamera(camera_);

	targetMesh_ = sphere;
	const std::array<glm::vec3, 3> targetColors{{
		{1.0f, 0.1f, 0.1f}, {0.1f, 0.35f, 1.0f}, {0.1f, 1.0f, 0.1f}
	}};
	for (std::size_t index = 0; index < targetMaterials_.size(); ++index)
	{
		targetMaterials_[index] = std::make_shared<Material>(shader);
		targetMaterials_[index]->color = targetColors[index];
	}

	lights_.push_back(Light::Point({100, 150, 110}, glm::vec3(1), 1.0f, 0.0f));
}

void ShootingScene::Update(float deltaTime, const Input& input)
{
	float horizontalChange = static_cast<float>(input.WasPressed(']')) - static_cast<float>(input.WasPressed('['));
	float verticalChange = static_cast<float>(input.WasPressed('=')) - static_cast<float>(input.WasPressed('-'));
	player_->m_fHorizontalSensitivity = std::clamp(player_->m_fHorizontalSensitivity + horizontalChange * 0.02f, 0.02f, 2.0f);
	player_->m_fVerticalSensitivity = std::clamp(player_->m_fVerticalSensitivity + verticalChange * 0.02f, 0.02f, 2.0f);

	if (network_ != nullptr)
	{
		if (IsPreparing() || roomState_.state == Net::GameState::Results)
			UpdateReadyInput(input);

		bool playing = roomState_.state == Net::GameState::Playing;
		if (playing)
		{
			if (input.WasPressed('r') || input.WasPressed('R'))
				gun_->StartReload();
			ApplyPlayerCommand(ReadPlayerInput(input), deltaTime);
		}
		Scene::Update(deltaTime, input);
		if (playing)
		{
			float interval = 1.0f / Net::ServerTickRate;
			movementSendElapsed_ += std::max(deltaTime, 0.0f);
			if (movementSendElapsed_ >= interval || hasShot_)
			{
				SendPlayerTransform();
				movementSendElapsed_ = std::fmod(movementSendElapsed_, interval);
			}
			Shoot();
			AdvanceRemoteTargets(deltaTime);
		}
		return;
	}

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

	bool wasPlaying = stages_.GetState() == StageState::Playing;
	const StageEvents events = stages_.Update(deltaTime);

	if (events.clearTargets)
	{
		ClearTargets();
		targetSpawnTime_ = 0.0f;
		hitIndicatorUntilMs_ = 0;
	}

	if (input.WasPressed('r') || input.WasPressed('R'))
	{
		gun_->StartReload();
	}

	ApplyPlayerCommand(ReadPlayerInput(input), deltaTime);
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
}

void ShootingScene::ApplyPlayerCommand(const PlayerCommand& command, float deltaTime)
{
	player_->ApplyCommand(command, deltaTime, *this);
	if (network_ != nullptr)
	{
		Net::PlayerPose pose = player_->GetNetworkPose();
		//클라에서 먼저 계산한 이동도 서버가 배정한 구역 안으로 제한. 카메라와 발사 원점은 이 위치를 사용.
		pose.position = ClampToPlayerArea(pose.position);
		player_->ApplyNetworkPose(pose);
	}
	player_->UpdateCamera(camera_);

	if (command.fire && gun_->Fire())
	{
		shot_ = {camera_.position, camera_.GetForward()};
		shotTransform_ = glm::translate(glm::mat4(1.0f), player_->transform.position) * camera_.GetRotation();
		hasShot_ = true;
	}
}

void ShootingScene::SpawnTargets()
{
	constexpr float SpawnHalfSize = TargetAreaHalfSize - TargetScale;
	std::uniform_real_distribution<float> horizontal(-SpawnHalfSize, SpawnHalfSize);
	std::uniform_real_distribution<float> height(0.5f, 2.0f);

	std::unique_ptr<Target> target = std::make_unique<Target>(targetMesh_, targetMaterials_.front());
	target->transform.position = {horizontal(random_), height(random_), horizontal(random_)};
	target->transform.scale = glm::vec3(TargetScale);
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
			uint32_t id = (*it)->GetId();
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
	for (Target* target : targets_)
	{
		RemoveObject(target->GetId());
	}

	targets_.clear();
}

void ShootingScene::Shoot()
{
	if (!hasShot_) return;
	hasShot_ = false;
	if (network_ != nullptr)
	{
		SendShoot();
		return;
	}

	// 원본은 마지막 광선을 유지하고 매 업데이트마다 등록 순서의 첫 표적을 검사합니다.

	for (Target* target : targets_)
	{
		float distance = 0.0f;

		if (target->active && Collision::Raycast(shot_, target->GetWorldSphere(), distance) && target->Hit())
		{
			stages_.AddScore();
			hitIndicatorUntilMs_ = GetTickCount64() + HitIndicatorDurationMs;
			break;
		}
	}
}

bool ShootingScene::IsPreparing() const
{
	return network_ && roomState_.state == Net::GameState::Preparing;
}

const Net::PlayerData* ShootingScene::FindLocalRoomPlayer() const
{
	for (const Net::PlayerData& player : roomState_.vPlayers)
		if (player.m_sessionToken == sessionToken_)
			return &player;
	return nullptr;
}

void ShootingScene::UpdateReadyInput(const Input& input)
{
	bool toggle = IsPreparing() && input.readyToggled;
	bool nextMatch = roomState_.state == Net::GameState::Results && (input.WasPressed('r') || input.WasPressed('R'));
	if (!toggle && !nextMatch)
		return;

	const Net::PlayerData* player = FindLocalRoomPlayer();
	if (!player || !player->mapLoaded || !mapLoadedSent_ || !network_->IsConnected())
		return;

	bool lastReady = pendingReadyChanges_.empty() ? player->ready == Net::ReadyState::Ready : pendingReadyChanges_.back().ready;
	if (nextMatch && lastReady)
		return;
	Net::ReadyChangeRequest request;
	request.roomId = roomState_.version.context.roomId;
	request.ready = lastReady ? 0 : 1;
	std::uint32_t requestId = network_->NextRequestId();
	Net::PacketData packet;
	if (!Net::PacketCodec::EncodeTcp(request, requestId, packet) || !network_->SendPacket(packet))
		throw std::runtime_error("준비 상태 변경 요청을 보내지 못했습니다.");
	pendingReadyChanges_.push({requestId, request.ready != 0});
	readyChangeRejected_ = false;
}

void ShootingScene::ConfirmReadyChange()
{
	if (pendingReadyChanges_.empty())
		return;
	const Net::PlayerData* player = FindLocalRoomPlayer();
	if (player && pendingReadyChanges_.front().ready == (player->ready == Net::ReadyState::Ready))
		pendingReadyChanges_.pop();
}

void ShootingScene::OnReadyRejected(std::uint32_t requestId)
{
	if (pendingReadyChanges_.empty() || requestId != pendingReadyChanges_.front().requestId)
		return;
	pendingReadyChanges_.pop();
	readyChangeRejected_ = pendingReadyChanges_.empty();
	// 거절 뒤 다음 요청이 이미 서버 상태와 같으면 서버는 변경 방송을 보내지 않는다.
	ConfirmReadyChange();
}

Hud ShootingScene::ShowReadyUI() const
{
	Hud hud;
	hud.crosshair = false;
	hud.lines.push_back("Room " + std::to_string(roomState_.version.context.roomId) + "    "
		+ std::to_string(roomState_.vPlayers.size()) + "/" + std::to_string(roomState_.capacity));
	hud.readyButton = "Loading...";
	for (const Net::PlayerData& player : roomState_.vPlayers)
	{
		bool isLocal = player.m_sessionToken == sessionToken_;
		std::string label = "Area " + std::to_string(player.areaId + 1);
		if (isLocal)
			label += " (you)";
		if (!player.mapLoaded)
			label += " - Loading";
		else if (player.ready == Net::ReadyState::Ready)
			label += " - Ready";
		else
			label += " - Waiting";
		hud.lines.push_back(label);
		if (isLocal && player.mapLoaded)
			hud.readyButton = player.ready == Net::ReadyState::Ready ? "Cancel ready [Space]" : "Ready [Space]";
	}
	if (!pendingReadyChanges_.empty())
	{
		hud.readyButton = pendingReadyChanges_.back().ready ? "Requesting ready..." : "Cancelling ready...";
		hud.message = "Click or press Space to change again";
	}
	else if (readyChangeRejected_)
		hud.message = "Server declined the change. Try again.";
	else
		hud.message = "Starts when everyone is ready";
	return hud;
}

Hud ShootingScene::GetHud() const
{
	if (IsPreparing())
		return ShowReadyUI();

	if (network_ != nullptr && roomState_.state == Net::GameState::Results)
	{
		Hud hud;
		hud.crosshair = false;
		hud.showResults = true;
		hud.message = "Match results (hits - shots / 2)";
		for (const Net::PlayerResult& player : finalResults_)
		{
			std::string marker = player.sessionToken == sessionToken_ ? "-> " : "   ";
			double score = static_cast<double>(player.stats.score) - player.stats.shotsFired / 2.0;
			std::ostringstream total;
			total.precision(1);
			total << std::fixed << score;
			hud.lines.push_back(marker + std::to_string(player.rank)
				+ ". hits: " + std::to_string(player.stats.score)
				+ " | shots: " + std::to_string(player.stats.shotsFired)
				+ " | total: " + total.str());
		}
		if (finalResults_.empty())
			hud.lines.push_back("Waiting for results...");
		std::size_t readyCount = 0;
		bool localReady = false;
		for (const Net::PlayerData& player : roomState_.vPlayers)
		{
			if (player.ready == Net::ReadyState::Ready)
			{
				++readyCount;
				if (player.m_sessionToken == sessionToken_)
					localReady = true;
			}
		}
		std::string readyStatus = std::to_string(readyCount) + "/" + std::to_string(roomState_.vPlayers.size());
		hud.lines.push_back((localReady || !pendingReadyChanges_.empty()) ? "Ready requested. Waiting for everyone (" + readyStatus + ")"
			: "Press R to ready for next match (" + readyStatus + ")");
		return hud;
	}

	std::ostringstream sensitivity;
	sensitivity.precision(2);
	sensitivity << std::fixed << "  sens H: " << player_->m_fHorizontalSensitivity
		<< " V: " << player_->m_fVerticalSensitivity;
	std::string sensitivityLabel = sensitivity.str();
	Hud hud;
	Net::ServerTimeMs nowMs = GetTickCount64();
	hud.hitIndicatorAlpha = hitIndicatorUntilMs_ > nowMs
		? static_cast<float>(hitIndicatorUntilMs_ - nowMs) / HitIndicatorDurationMs : 0.0f;
	if (network_ != nullptr)
	{
		hud.lines = {"hits: " + std::to_string(networkStats_.score),
			"shots: " + std::to_string(networkStats_.shotsFired) + sensitivityLabel};
		if (roomState_.state == Net::GameState::Playing)
		{
			Net::ServerTimeMs remainingMs = endsAtMs_ > serverTimeMs_ ? endsAtMs_ - serverTimeMs_ : 0;
			Net::ServerTimeMs elapsedMs = GetTickCount64() - matchTimeReceivedAtMs_;
			remainingMs = elapsedMs < remainingMs ? remainingMs - elapsedMs : 0;
			hud.lines.push_back("timer: " + std::to_string((remainingMs + 999) / 1000));
		}
		if (gun_ != nullptr)
		{
			hud.lines.push_back(gun_->IsReloading()
				? "reloading"
				: "ammo: " + std::to_string(gun_->GetAmmo()) + "/" + std::to_string(Net::MagazineCapacity));
		}
		return hud;
	}
	hud.lines = {"stage: " + std::to_string(stages_.GetStageNumber()), "point: " + std::to_string(stages_.GetScore()) + sensitivityLabel,
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


bool ShootingScene::SetNetworkSession(NetworkClient& network, const Net::RoomEnterResult& result)
{
	Net::SessionToken token = network.GetSessionToken();
	if (!Net::IsValidSessionToken(token) || result.result != Net::ErrorCode::Ok
		|| result.room.version.context.roomId == 0 || result.membershipId == 0)
		return false;

	bool hasLocalPlayer = false;
	for (const Net::PlayerData& player : result.room.vPlayers)
		if (player.m_sessionToken == token)
			hasLocalPlayer = true;
	if (!hasLocalPlayer)
		return false;

	network_ = &network;
	sessionToken_ = token;
	membershipId_ = result.membershipId;
	roomState_ = result.room;
	pendingReadyChanges_ = {};
	readyChangeRejected_ = false;
	mapLoadedSent_ = false;
	startedMatchId_ = 0;
	serverTimeMs_ = startsAtMs_ = endsAtMs_ = 0;
	hitIndicatorUntilMs_ = 0;
	finalResults_.clear();
	return true;
}

void ShootingScene::SendMapLoaded()
{
	if (mapLoadedSent_)
		return;
	if (network_ == nullptr || !network_->IsConnected()
		|| roomState_.version.context.roomId == 0 || membershipId_ == 0)
		throw std::runtime_error("맵 로딩 완료를 보낼 접속 정보가 없습니다.");

	// 장면 초기화가 끝난 메인 스레드에서 한 번만 보낸다.
	Net::MapLoaded loaded;
	loaded.roomId = roomState_.version.context.roomId;
	Net::PacketData packet;
	if (!Net::PacketCodec::EncodeTcp(loaded, network_->NextRequestId(), packet)
		|| !network_->SendPacket(packet))
		throw std::runtime_error("맵 로딩 완료를 보내지 못했습니다.");
	mapLoadedSent_ = true;
}

void ShootingScene::OnRoomStateChanged(const Net::RoomStateData& state)
{
	if (network_ == nullptr || state.version.context.roomId != roomState_.version.context.roomId
		|| state.version.context.matchId != roomState_.version.context.matchId
		|| (state.version.revision != roomState_.version.revision
			&& !Net::IsNewerSequence(state.version.revision, roomState_.version.revision)))
		return;

	roomState_ = state;
	ConfirmReadyChange();
	RemoveDepartedRemotePlayers();
	if (state.state == Net::GameState::Results)
		ApplyTargetSnapshot({});
}

void ShootingScene::OnGameStarted(const Net::GameStarted& event)
{
	Net::MatchContext current = roomState_.version.context;
	Net::MatchId matchId = event.version.context.matchId;
	if (network_ == nullptr || !mapLoadedSent_ || event.version.context.roomId != current.roomId
		|| matchId == 0 || matchId == startedMatchId_
		|| (current.matchId != 0 && matchId != current.matchId
			&& !Net::IsNewerSequence(matchId, current.matchId))
		|| (event.version.revision != roomState_.version.revision
			&& !Net::IsNewerSequence(event.version.revision, roomState_.version.revision)))
		return;

	roomState_.version = event.version;
	roomState_.state = Net::GameState::Playing;
	roomState_.rules = event.rules;
	roomState_.vPlayers = event.players;
	startedMatchId_ = matchId;
	pendingReadyChanges_ = {};
	readyChangeRejected_ = false;
	gun_->ResetGame();
	//경기가 바뀌면 이전 경기의 이동 순번과 보정 기준을 같이 초기화.
	inputSequence_ = lastAppliedInput_ = 0;
	sentPositions_ = {};
	hasAppliedInput_ = false;
	movementSendElapsed_ = 0.0f;
	nextShotId_ = 1;
	lastResolvedShotId_ = 0;
	hitIndicatorUntilMs_ = 0;
	networkStats_ = {};
	hasShot_ = false;
	ClearTargets();
	ApplyTargetSnapshot({});
	finalResults_.clear();
	for (const PlayerModel& remote : remotePlayers_)
		RemovePlayerModel(remote);
	remotePlayers_.clear();

	for (const Net::PlayerData& entry : event.players)
	{
		if (entry.m_sessionToken == sessionToken_)
		{
			networkStats_ = entry.stats;
			player_->ApplyNetworkPose(entry.pose);
			player_->UpdateCamera(camera_);
			continue;
		}
		//상대도 같은 몸통·머리·총기를 생성. 구역별 경기이므로 상대 몸체는 벽으로 취급하지 않음.
		const PlayerModel remote = AddPlayer(entry.m_sessionToken, remotePlayerMaterial_, false);
		remote.body->ApplyNetworkPose(entry.pose);
		remotePlayers_.push_back(remote);
	}
	//첫 이동 요청 전에는 서버가 처리한 순번이 0이므로, 시작 위치도 비교 기준으로 저장.
	sentPositions_[0] = {0, player_->transform.position, true};
	serverTimeMs_ = event.serverTimeMs;
	startsAtMs_ = event.startsAtMs;
	endsAtMs_ = event.endsAtMs;
	matchTimeReceivedAtMs_ = GetTickCount64();
	std::cout << "Room " << current.roomId << " match " << matchId << " started ("
		<< event.players.size() << " players)." << std::endl;
}

void ShootingScene::RemoveDepartedRemotePlayers()
{
	//퇴장한 상대는 TCP 명단에서 빠진 시점에 제거. 늦은 UDP는 표시 객체를 다시 만들지 않음.
	for (auto remote = remotePlayers_.begin(); remote != remotePlayers_.end();)
	{
		const auto& players = roomState_.vPlayers;
		auto member = std::find_if(players.begin(), players.end(), [&](const Net::PlayerData& entry)
		{
			return entry.m_sessionToken == remote->body->m_SessionToken;
		});
		if (member != players.end())
		{
			++remote;
			continue;
		}

		RemovePlayerModel(*remote);
		remote = remotePlayers_.erase(remote);
	}
}

void ShootingScene::ApplyTargetPositions(const Net::UdpHeader& header, const Net::TargetSnapshot& snapshot)
{
	const Net::MatchContext& context = roomState_.version.context;
	if (roomState_.state != Net::GameState::Playing || header.context.roomId != context.roomId
		|| header.context.matchId != context.matchId || header.membershipId != membershipId_
		|| header.m_sessionToken != sessionToken_)
		return;

	ApplyTargetSnapshot(snapshot);
}

void ShootingScene::ApplyTargetSnapshot(const Net::TargetSnapshot& snapshot)
{
	while (remoteTargets_.size() > snapshot.targetCount)
	{
		RemoveObject(remoteTargets_.back().object->GetId());
		remoteTargets_.pop_back();
	}

	for (std::size_t index = 0; index < snapshot.targetCount; ++index)
	{
		const Net::TargetMotionData& motion = snapshot.targets[index];
		const std::shared_ptr<Material>& material = targetMaterials_[static_cast<std::size_t>(motion.kind)];
		float radius = motion.kind == Net::TargetKind::Stationary
			? ShootingRange::StationaryTargetRadius : ShootingRange::TargetRadius;
		if (index == remoteTargets_.size())
		{
			Object* object = AddProp("Remote target", targetMesh_, material, motion.position, glm::vec3(radius));
			remoteTargets_.push_back({object, motion});
		}

		RemoteTarget& target = remoteTargets_[index];
		if (target.motion.kind != motion.kind)
		{
			// 표시 슬롯의 종류가 바뀌면 올바른 색상의 표시 객체로 교체한다.
			Object* object = AddProp("Remote target", targetMesh_, material, motion.position, glm::vec3(radius));
			RemoveObject(target.object->GetId());
			target.object = object;
		}
		target.motion = motion;
		target.object->transform.position = motion.position;
		target.object->transform.scale = glm::vec3(radius);
	}
}

void ShootingScene::AdvanceRemoteTargets(float deltaSeconds)
{
	if (deltaSeconds <= 0.0f) return;
	float angle = ShootingRange::OrbitAngularSpeed * deltaSeconds;
	float cosine = std::cos(angle);
	float sine = std::sin(angle);
	for (RemoteTarget& target : remoteTargets_)
	{
		Net::TargetMotionData& motion = target.motion;
		if (motion.kind == Net::TargetKind::Orbiting)
		{
			// 접선 속도를 회전시켜 패킷 사이에도 같은 수평 원 궤도를 따라간다.
			glm::vec3 previousVelocity = motion.velocity;
			motion.velocity.x = previousVelocity.x * cosine - previousVelocity.z * sine;
			motion.velocity.z = previousVelocity.x * sine + previousVelocity.z * cosine;
			motion.position.x += (motion.velocity.z - previousVelocity.z) / ShootingRange::OrbitAngularSpeed;
			motion.position.z -= (motion.velocity.x - previousVelocity.x) / ShootingRange::OrbitAngularSpeed;
		}
		else
		{
			motion.position += motion.velocity * deltaSeconds;
		}
		target.object->transform.position = motion.position;
	}
}

void ShootingScene::UpdateMatchTime(const Net::MatchTime& time)
{
	const Net::MatchContext& context = roomState_.version.context;
	if (roomState_.state != Net::GameState::Playing || time.context.roomId != context.roomId
		|| time.context.matchId != context.matchId || time.serverTimeMs < serverTimeMs_)
		return;

	serverTimeMs_ = time.serverTimeMs;
	endsAtMs_ = time.endsAtMs;
	matchTimeReceivedAtMs_ = GetTickCount64();
}

void ShootingScene::RecvGameResults(const Net::GameResults& result)
{
	const Net::RoomVersion& current = roomState_.version;
	const Net::RoomVersion& received = result.room.version;
	if (received.context.roomId != current.context.roomId || received.context.matchId != current.context.matchId
		|| (received.revision != current.revision && !Net::IsNewerSequence(received.revision, current.revision)))
		return;

	OnRoomStateChanged(result.room);
	endsAtMs_ = result.endedAtMs;
	finalResults_ = result.players;
	for (const Net::PlayerResult& player : finalResults_)
	{
		if (player.sessionToken == sessionToken_)
		{
			networkStats_ = player.stats;
			break;
		}
	}
}

PlayerCommand ShootingScene::ReadPlayerInput(const Input& input) const
{
	PlayerCommand command;
	command.movement.x = static_cast<float>(input.IsDown('d')) - static_cast<float>(input.IsDown('a'));
	command.movement.y = static_cast<float>(input.IsDown('s')) - static_cast<float>(input.IsDown('w'));
	command.lookDelta = input.mouseDelta;
	command.fire = input.firePressed;
	return command;
}

void ShootingScene::SendPlayerTransform()
{
	if (network_ == nullptr || roomState_.state != Net::GameState::Playing || !network_->IsConnected())
		return;
	Net::UdpPacket<Net::PlayerTransform> request;
	request.header.type = Net::UdpType::PlayerTransform;
	request.header.m_sessionToken = sessionToken_;
	request.header.context = roomState_.version.context;
	request.header.membershipId = membershipId_;
	request.header.sequence = ++inputSequence_;
	request.packetData.inputSequence = inputSequence_;
	request.packetData.pose = player_->GetNetworkPose();
	Net::PacketData packet;
	if (!Net::PacketCodec::EncodeUdp(request, packet) || !network_->SendPacket(packet))
		throw std::runtime_error("Failed to send player pose");
	//서버가 돌려준 순번으로 전송 당시 위치를 찾기 위해 저장. 나머지 연산으로 최근 64칸을 재사용.
	SentPosition& sent = sentPositions_[inputSequence_ % sentPositions_.size()];
	sent = {inputSequence_, request.packetData.pose.position, true};
}

void ShootingScene::RecvTransformData(const Net::PlayerSnapshot& snapshot)
{
	//해당 위치에 필요한 TCP 상태를 아직 못 받았다면 적용하지 않음.
	if (network_ == nullptr || roomState_.state != Net::GameState::Playing
		|| Net::IsNewerSequence(snapshot.requiredRevision, roomState_.version.revision))
		return;
	for (std::size_t index = 0; index < snapshot.playerCount; ++index)
	{
		const Net::PlayerTransformEntry& entry = snapshot.players[index];
		if (entry.m_sessionToken == sessionToken_)
		{
			CorrectLocalPlayerPosition(entry);
			continue;
		}

		//상대는 서버 확정 위치와 시선을 그대로 표시. 내 이동 요청 순번과 비교하지 않음.
		for (const PlayerModel& remote : remotePlayers_)
		{
			if (entry.m_sessionToken == remote.body->m_SessionToken)
			{
				remote.body->ApplyNetworkPose(entry.pose);
				break;
			}
		}
	}
}

void ShootingScene::CorrectLocalPlayerPosition(const Net::PlayerTransformEntry& entry)
{
	//보낸 적 없는 순번이거나 이미 적용한 응답이면 위치를 다시 보정하지 않음.
	if (Net::IsNewerSequence(entry.lastProcessedInput, inputSequence_)
		|| (hasAppliedInput_ && !Net::IsNewerSequence(entry.lastProcessedInput, lastAppliedInput_)))
		return;
	SentPosition& acknowledged = sentPositions_[entry.lastProcessedInput % sentPositions_.size()];
	//같은 배열 칸이 더 최근 요청으로 덮였을 수 있으므로, 유효 여부와 실제 순번을 모두 확인.
	if (!acknowledged.valid || acknowledged.sequence != entry.lastProcessedInput)
		return;

	//서버 확정 위치 - 해당 요청을 보낼 때의 위치 = 그 요청까지 발생한 위치 오차.
	//이 차이만 현재 위치에 더해서, 응답을 기다리는 동안 이동한 거리와 현재 시선은 유지.
	Net::PlayerPose previous = player_->GetNetworkPose();
	Net::PlayerPose corrected = previous;
	corrected.position = ClampToPlayerArea(previous.position + (entry.pose.position - acknowledged.position));
	if (!player_->ApplyNetworkPose(corrected))
		return;

	Box playerBox = player_->GetWorldBox();
	for (const std::unique_ptr<Object>& object : GetObjects())
	{
		if (!object->active || !object->blocksMovement || object->collider != ColliderType::Box)
			continue;
		Box wall = object->GetWorldBox();
		if (playerBox.min.x < wall.max.x && playerBox.max.x > wall.min.x
			&& playerBox.min.z < wall.max.z && playerBox.max.z > wall.min.z)
		{
			//보정한 위치가 벽 안이면 서버가 확정한 위치로 이동. yaw, pitch는 현재 클라 값을 유지.
			corrected.position = ClampToPlayerArea(entry.pose.position);
			player_->ApplyNetworkPose(corrected);
			break;
		}
	}

	//구역 제한과 벽 보정까지 끝난 뒤 실제로 이동한 보정량 계산.
	glm::vec3 correction = player_->transform.position - previous.position;
	for (SentPosition& sent : sentPositions_)
	{
		if (!sent.valid)
			continue;
		//서버가 처리했다고 확인된 요청은 비움. 아직 확인 못한 요청의 기준 위치도 같은 만큼 옮김.
		//안 옮기면 다음 응답에서 이번에 반영한 오차를 다시 적용할 수 있음.
		if (!Net::IsNewerSequence(sent.sequence, entry.lastProcessedInput))
			sent.valid = false;
		else
			sent.position += correction;
	}
	lastAppliedInput_ = entry.lastProcessedInput;
	hasAppliedInput_ = true;
	player_->UpdateCamera(camera_);
}

void ShootingScene::SendShoot()
{
	if (network_ == nullptr || roomState_.state != Net::GameState::Playing || !network_->IsConnected())
		return;
	Net::FireRequest request;
	request.context = roomState_.version.context;
	request.membershipId = membershipId_;
	request.shotId = nextShotId_++;
	request.inputSequence = inputSequence_;
	request.shooterTransform = shotTransform_;
	request.origin = shot_.origin;
	Net::PacketData packet;
	if (!Net::PacketCodec::EncodeTcp(request, network_->NextRequestId(), packet) || !network_->SendPacket(packet))
		throw std::runtime_error("Failed to send shot");
}

void ShootingScene::RecvShootResult(const Net::ShotResolved& result)
{
	Net::MatchContext context = roomState_.version.context;
	if (result.version.context.roomId != context.roomId || result.version.context.matchId != context.matchId
		|| (result.version.revision != roomState_.version.revision
			&& !Net::IsNewerSequence(result.version.revision, roomState_.version.revision)))
		return;
	//이미 받은 발사 결과는 방의 점수와 HUD를 바꾸기 전에 걸러서 과거 점수로 돌아가지 않게 함.
	if (result.shooterToken == sessionToken_ && (result.shotId == 0
		|| (lastResolvedShotId_ != 0 && !Net::IsNewerSequence(result.shotId, lastResolvedShotId_))))
		return;
	roomState_.version = result.version;
	for (Net::PlayerData& player : roomState_.vPlayers)
		if (player.m_sessionToken == result.shooterToken)
			player.stats = result.shooterStats;
	if (result.shooterToken != sessionToken_)
		return;
	networkStats_ = result.shooterStats;
	if (result.outcome == Net::ShotOutcome::HitTarget)
		hitIndicatorUntilMs_ = GetTickCount64() + HitIndicatorDurationMs;
	lastResolvedShotId_ = result.shotId;
}

//플레이어 이동 구역에 따라 이동 범위를 clamp해주는 함수
glm::vec3 ShootingScene::ClampToPlayerArea(glm::vec3 position) const
{
	//본인 토큰으로 배정된 구역을 찾음. 표적 구역이나 다른 플레이어 구역으로 제한하면 안 됨.
	for (const Net::PlayerData& player : roomState_.vPlayers)
		if (player.m_sessionToken == sessionToken_)
			for (const Net::AreaData& area : roomState_.vAreas)
				if (area.areaId == player.areaId)
				{
					//위치는 플레이어 중심이므로 min에는 몸체 절반을 더하고 max에서는 빼서 몸이 경계를 넘지 않게 함.
					position.x = std::clamp(position.x, area.movementBounds.min.x + Net::PlayerHalfExtent,
						area.movementBounds.max.x - Net::PlayerHalfExtent);
					position.z = std::clamp(position.z, area.movementBounds.min.z + Net::PlayerHalfExtent,
						area.movementBounds.max.z - Net::PlayerHalfExtent);
					return position;
				}
	return position;
}
