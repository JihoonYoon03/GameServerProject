#include "pch.h"
#include "ShootingScene.h"
#include "NetworkClient.h"
#include "../Shared/PacketCodec.h"
#include "../Shared/ShootingRange.h"

namespace
{
	constexpr float TargetAreaHalfSize = ShootingRange::TargetHalfSize;
	constexpr float TargetScale = 0.3f;
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

	std::unique_ptr<Player> player = std::make_unique<Player>(cube, playerMaterial);
	player_ = player.get();
	player_->m_SessionToken = sessionToken_;
	player->name = "Local player";
	player->transform.position = southArea + glm::vec3(0, 1, 0);
	player->transform.scale = glm::vec3(Net::PlayerHalfExtent);
	player->collider = ColliderType::Box;
	AddObject(std::move(player));
	for (const Net::PlayerData& entry : roomState_.vPlayers)
		if (entry.m_sessionToken == sessionToken_)
			player_->ApplyNetworkPose(entry.pose);
	player_->UpdateCamera(camera_);

	std::shared_ptr<Material> gunMaterial = std::make_shared<Material>(shader);
	gunMaterial->color = glm::vec3(0.2f);

	std::unique_ptr<Gun> gun = std::make_unique<Gun>(assets_.LoadMesh("Resources/Meshes/Pistol.obj"), gunMaterial);
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
	float horizontalChange = static_cast<float>(input.WasPressed(']')) - static_cast<float>(input.WasPressed('['));
	float verticalChange = static_cast<float>(input.WasPressed('=')) - static_cast<float>(input.WasPressed('-'));
	player_->m_fHorizontalSensitivity = std::clamp(player_->m_fHorizontalSensitivity + horizontalChange * 0.02f, 0.02f, 2.0f);
	player_->m_fVerticalSensitivity = std::clamp(player_->m_fVerticalSensitivity + verticalChange * 0.02f, 0.02f, 2.0f);

	if (network_ != nullptr)
	{
		if (input.WasPressed('1') && roomState_.state == Net::GameState::Preparing
			&& mapLoadedSent_ && network_->IsConnected())
		{
			Net::StartGameRequest request{roomState_.version.context.roomId};
			Net::PacketData packet;
			if (!Net::PacketCodec::EncodeTcp(request, network_->NextRequestId(), packet)
				|| !network_->SendPacket(packet))
				throw std::runtime_error("Failed to send match start request");
		}

		bool playing = roomState_.state == Net::GameState::Playing;
		if (playing)
			ApplyPlayerCommand(ReadPlayerInput(input), deltaTime);
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
		gun_->FollowCamera(camera_, player_->transform.position);
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
	gun_->FollowCamera(camera_, player_->transform.position);
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

	std::unique_ptr<Target> target = std::make_unique<Target>(targetMesh_, targetMaterial_);
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
			break;
		}
	}
}

Hud ShootingScene::GetHud() const
{
	std::ostringstream sensitivity;
	sensitivity.precision(2);
	sensitivity << std::fixed << "  sens H: " << player_->m_fHorizontalSensitivity
		<< " V: " << player_->m_fVerticalSensitivity;
	std::string sensitivityLabel = sensitivity.str();
	Hud hud;
	if (network_ != nullptr)
	{
		hud.lines = {"point: " + std::to_string(networkStats_.score),
			"shots: " + std::to_string(networkStats_.shotsFired) + sensitivityLabel};
		if (roomState_.state == Net::GameState::Preparing)
			hud.message = "Press 1 to start";
		else if (roomState_.state == Net::GameState::Results)
		{
			hud.lines.push_back("timer: 0");
			if (finalRank_ != 0)
				hud.lines.push_back("rank: " + std::to_string(finalRank_));
			hud.message = "Match finished";
		}
		else if (lastResolvedShotId_ != 0)
			hud.message = lastShotOutcome_ == Net::ShotOutcome::HitTarget ? "Hit"
				: (lastShotOutcome_ == Net::ShotOutcome::BlockedByWorld ? "Blocked" : "Miss");
		if (roomState_.state == Net::GameState::Playing)
		{
			Net::ServerTimeMs remainingMs = endsAtMs_ > serverTimeMs_ ? endsAtMs_ - serverTimeMs_ : 0;
			Net::ServerTimeMs elapsedMs = GetTickCount64() - matchTimeReceivedAtMs_;
			remainingMs = elapsedMs < remainingMs ? remainingMs - elapsedMs : 0;
			hud.lines.push_back("timer: " + std::to_string((remainingMs + 999) / 1000));
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
	mapLoadedSent_ = false;
	startedMatchId_ = 0;
	serverTimeMs_ = startsAtMs_ = endsAtMs_ = 0;
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
	if (state.state == Net::GameState::Results)
		ApplyTargetSnapshot({});
	// 다인 참가자·구역 표시는 후속 방 확장 일정에서 연결한다.
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
	//경기가 바뀌면 이전 경기의 이동 순번과 보정 기준을 같이 초기화.
	inputSequence_ = lastAppliedInput_ = 0;
	sentPositions_ = {};
	hasAppliedInput_ = false;
	movementSendElapsed_ = 0.0f;
	nextShotId_ = 1;
	lastResolvedShotId_ = 0;
	networkStats_ = {};
	hasShot_ = false;
	ClearTargets();
	ApplyTargetSnapshot({});
	finalRank_ = 0;
	for (const Net::PlayerData& entry : event.players)
		if (entry.m_sessionToken == sessionToken_)
		{
			networkStats_ = entry.stats;
			player_->ApplyNetworkPose(entry.pose);
			player_->UpdateCamera(camera_);
		}
	//첫 이동 요청 전에는 서버가 처리한 순번이 0이므로, 시작 위치도 비교 기준으로 저장.
	sentPositions_[0] = {0, player_->transform.position, true};
	serverTimeMs_ = event.serverTimeMs;
	startsAtMs_ = event.startsAtMs;
	endsAtMs_ = event.endsAtMs;
	matchTimeReceivedAtMs_ = GetTickCount64();
	std::cout << "서버의 1인 경기 시작 응답을 반영했습니다.\n";
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
		if (index == remoteTargets_.size())
		{
			Object* object = AddProp("Remote target", targetMesh_, targetMaterial_, motion.position, glm::vec3(TargetScale));
			remoteTargets_.push_back({object, motion.velocity});
		}
		else
		{
			remoteTargets_[index].object->transform.position = motion.position;
			remoteTargets_[index].velocity = motion.velocity;
		}
	}
}

void ShootingScene::AdvanceRemoteTargets(float deltaSeconds)
{
	for (const RemoteTarget& target : remoteTargets_)
		target.object->transform.position += target.velocity * deltaSeconds;
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
	for (const Net::PlayerResult& player : result.players)
	{
		if (player.sessionToken == sessionToken_)
		{
			networkStats_ = player.stats;
			finalRank_ = player.rank;
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
		if (entry.m_sessionToken != sessionToken_)
			continue;
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
		return;
	}
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
	lastShotOutcome_ = result.outcome;
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
