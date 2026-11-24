#include "pch.h"
#include "Framework.h"
#include "NetworkClient.h"
#include "MainScene.h"
#include "ShootingScene.h"
#include "../Shared/PacketCodec.h"

Framework* Framework::current_ = nullptr;

Framework::Framework(std::unique_ptr<NetworkClient> network)
	: network_(std::move(network))
{
}

Framework::~Framework() = default;

namespace
{
	constexpr int UpdateIntervalMs = 1000 / 240;
	constexpr float ReadyLeft = -0.80f;
	constexpr float ReadyRight = 0.08f;
	constexpr float ReadyBottom = -0.45f;
	constexpr float ReadyTop = -0.25f;
	constexpr float LeaveLeft = -0.90f;
	constexpr float LeaveRight = -0.22f;
	constexpr float LeaveBottom = -0.94f;
	constexpr float LeaveTop = -0.76f;

	//LegacyGL
	void DrawText(const std::string& text, float x, float y, float scale)
	{
		//LegacyGL
		glPushMatrix();
		//LegacyGL
		glTranslatef(x, y, 0.0f);
		//LegacyGL
		glScalef(scale, scale, scale);

		for (unsigned char character : text)
		{
			// GLUT drawing helper used with the legacy fixed-function pipeline here.
			//LegacyGL
			glutStrokeCharacter(GLUT_STROKE_ROMAN, character);
		}

		//LegacyGL
		glPopMatrix();
	}

	float ReadyTextWidth(const std::string& text)
	{
		return static_cast<float>(glutStrokeLength(GLUT_STROKE_ROMAN,
			reinterpret_cast<const unsigned char*>(text.c_str())));
	}

	void DrawLobbyPanel(const Hud& hud)
	{
		glColor3f(0.10f, 0.13f, 0.17f);
		glRectf(-0.93f, -0.89f, 0.93f, 0.90f);
		glLineWidth(1.7f);
		glColor3f(0.94f, 0.96f, 0.98f);
		DrawText(hud.lines.front(), -0.82f, 0.68f, 0.00070f);
		DrawText("W/S: select", -0.82f, 0.565f, 0.00039f);
		for (const HudButton& button : hud.lobbyButtons)
		{
			const HudRect& bounds = button.bounds;
			if (!button.enabled) glColor3f(0.17f, 0.19f, 0.22f);
			else if (button.selected) glColor3f(0.53f, 0.86f, 0.76f);
			else glColor3f(0.22f, 0.28f, 0.34f);
			glRectf(bounds.left, bounds.bottom, bounds.right, bounds.top);

			if (!button.enabled) glColor3f(0.66f, 0.69f, 0.72f);
			else if (button.selected) glColor3f(0.04f, 0.12f, 0.11f);
			else glColor3f(0.94f, 0.96f, 0.98f);
			float scale = std::min(0.00055f, (bounds.right - bounds.left - 0.08f) / ReadyTextWidth(button.label));
			DrawText(button.label, bounds.left + 0.04f, (bounds.top + bounds.bottom) * 0.5f - 0.025f, scale);
		}
		if (!hud.message.empty())
		{
			glColor3f(0.94f, 0.96f, 0.98f);
			DrawText(hud.message, -0.82f, -0.52f, 0.00052f);
		}
	}

	void DrawReadyPanel(const Hud& hud)
	{
		glColor3f(0.10f, 0.13f, 0.17f);
		glRectf(-0.90f, -0.70f, 0.46f, 0.82f);
		glLineWidth(1.7f);
		float y = 0.64f;
		for (std::size_t i = 0; i < hud.lines.size(); ++i)
		{
			float scale = i == 0 ? 0.00072f : 0.00050f;
			glColor3f(0.94f, 0.96f, 0.98f);
			DrawText(hud.lines[i], ReadyLeft, y, std::min(scale, 1.16f / std::max(1.0f, ReadyTextWidth(hud.lines[i]))));
			y -= i == 0 ? 0.21f : 0.14f;
		}

		glColor3f(0.53f, 0.86f, 0.76f);
		glRectf(ReadyLeft, ReadyBottom, ReadyRight, ReadyTop);
		float width = std::max(1.0f, ReadyTextWidth(hud.readyButton));
		float scale = std::min(0.00064f, (ReadyRight - ReadyLeft - 0.08f) / width);
		glColor3f(0.04f, 0.12f, 0.11f);
		DrawText(hud.readyButton, (ReadyLeft + ReadyRight - width * scale) / 2.0f, -0.38f, scale);

		glColor3f(0.83f, 0.87f, 0.90f);
		DrawText(hud.message, ReadyLeft, -0.60f, std::min(0.00046f, 1.16f / std::max(1.0f, ReadyTextWidth(hud.message))));
	}
} // namespace

int Framework::Run(int argc, char** argv, std::unique_ptr<Scene> scene)
{
	current_ = this;

	try
	{
		glutInit(&argc, argv);
		glutInitContextVersion(3, 3);
		// 기존 GLUT 글꼴과 디버그 선은 호환 프로파일에서 그립니다.
		// Compatibility context required by the legacy drawing paths below.
		//LegacyGL
		glutInitContextProfile(GLUT_COMPATIBILITY_PROFILE);
		glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_MULTISAMPLE);
		glutInitWindowSize(width_, height_);
		glutInitWindowPosition(100, 100);
		glutSetOption(GLUT_MULTISAMPLE, 4);
		window_ = glutCreateWindow("Example1");

		if (window_ <= 0)
		{
			throw std::runtime_error("Cannot create OpenGL window");
		}

		glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
		glewExperimental = GL_TRUE;

		if (glewInit() != GLEW_OK)
		{
			throw std::runtime_error("GLEW initialization failed");
		}

		while (glGetError() != GL_NO_ERROR)
		{
		} // GLEW 초기화 과정의 기존 오류 상태 제거

		if (!GLEW_VERSION_3_3)
		{
			throw std::runtime_error("OpenGL 3.3 is required");
		}

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glEnable(GL_MULTISAMPLE);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		scene_ = std::move(scene);
		if (network_ == nullptr)
		{
			scene_->Initialize();
			sceneInitialized_ = true;
		}

		glutDisplayFunc(DisplayCallback);
		glutReshapeFunc(ReshapeCallback);
		glutKeyboardFunc(KeyDownCallback);
		glutKeyboardUpFunc(KeyUpCallback);
		glutMouseFunc(MouseCallback);
		glutMotionFunc(MotionCallback);
		glutPassiveMotionFunc(MotionCallback);
		glutCloseFunc(CloseCallback);
		glutSetCursor(GLUT_CURSOR_NONE);
		previousTime_ = std::chrono::steady_clock::now();
		glutTimerFunc(UpdateIntervalMs, TimerCallback, 0);
		glutMainLoop();
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		exitCode_ = 1;
	}

	// OpenGL 컨텍스트가 살아 있을 때 GPU 자원을 먼저 해제합니다.
	scene_.reset();
	roomMenu_.reset();
	sceneInitialized_ = false;

	if (window_ > 0)
	{
		int window = window_;
		window_ = 0;
		glutDestroyWindow(window);
	}

	current_ = nullptr;

	return exitCode_;
}

void Framework::Tick()
{
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	float elapsed = std::chrono::duration<float>(now - previousTime_).count();
	previousTime_ = now;
	const bool menuCursorWasActive = menuCursorActive_;

	if (network_ != nullptr)
	{
		UpdateConnection();
		if (network_->IsDisconnected())
		{
			std::cerr << "서버 연결이 종료되었습니다.\n";
			exitCode_ = 1;
			Close();
			return;
		}
	}
	if (!sceneInitialized_)
	{
		input_.EndFrame();
		return;
	}

	if (roomMenu_ && (input_.leaveClicked || input_.WasPressed('l') || input_.WasPressed('L')))
		roomMenu_->ExitRoom();
	// 같은 방의 화면 전환에서는 퇴장 입력을 처리한 뒤 나머지 프레임 입력을 비운다.
	if (menuCursorWasActive != menuCursorActive_)
		input_.EndFrame();
	if (!roomMenu_ || !roomMenu_->IsLeaving())
		scene_->Update(elapsed, input_);
	input_.EndFrame();

	if (elapsed > 0.0f)
	{
		glutSetWindowTitle(("FPS: " + std::to_string(static_cast<int>(1.0f / elapsed))).c_str());
	}

	glutPostRedisplay();
}

void Framework::UpdateConnection()
{
	network_->UpdateNetwork();
	MainScene* lobby = dynamic_cast<MainScene*>(scene_.get());
	ShootingScene* shooting = dynamic_cast<ShootingScene*>(scene_.get());

	for (const Net::PacketData& packet : network_->GetReceivedPackets())
	{
		if (packet.transport == Net::PacketTransport::Udp)
		{
			Net::UdpHeader header;
			if (!Net::PacketCodec::ReadUdpHeader(packet.bytes.data(), packet.bytes.size(), header))
				continue;
			if (lobby && !sceneInitialized_ && header.type == Net::UdpType::RegisterResult && network_->IsConnected())
			{
				lobby->Initialize();
				sceneInitialized_ = true;
			}
			if (!sceneInitialized_ || !shooting)
				continue;

			const std::uint8_t* messageBytes = packet.bytes.data() + Net::UdpHeaderBytes;
			std::size_t messageSize = packet.bytes.size() - Net::UdpHeaderBytes;
			if (header.type == Net::UdpType::PlayerSnapshot)
			{
				Net::PlayerSnapshot snapshot;
				if (Net::PacketCodec::ReadMessage(header.type, messageBytes, messageSize, snapshot))
					shooting->RecvTransformData(snapshot);
			}
			else if (header.type == Net::UdpType::TargetSnapshot)
			{
				Net::TargetSnapshot snapshot;
				if (Net::PacketCodec::ReadMessage(header.type, messageBytes, messageSize, snapshot))
					shooting->ApplyTargetPositions(header, snapshot);
			}

			continue;
		}
		Net::TcpHeader header;
		if (Net::PacketCodec::ReadTcpHeader(packet.bytes.data(), packet.bytes.size(), header)
			!= Net::DecodeStatus::Complete)
			continue;
		const std::uint8_t* messageBytes = packet.bytes.data() + Net::TcpHeaderBytes;
		std::size_t messageSize = packet.bytes.size() - Net::TcpHeaderBytes;
		switch (header.type)
		{
		case Net::TcpType::RoomListResult:
		{
			Net::RoomListResult result;
			if (!Net::PacketCodec::ReadMessage(header.type, messageBytes, messageSize, result))
				throw std::runtime_error("방 목록 응답을 읽지 못했습니다.");
			if (lobby)
				lobby->UpdateRoomList(header.requestId, result);
			break;
		}
		case Net::TcpType::RoomEnterResult:
		{
			Net::RoomEnterResult result;
			if (!Net::PacketCodec::ReadMessage(header.type, messageBytes, messageSize, result))
				throw std::runtime_error("방 입장 응답을 읽지 못했습니다.");
			if (!lobby || !lobby->JoinRoom(result))
				break;
			auto nextScene = std::make_unique<ShootingScene>();
			shooting = nextScene.get();
			// 입장에 사용한 로비를 보관해 퇴장 후 같은 목록 화면으로 돌아간다.
			roomMenu_.reset(static_cast<MainScene*>(scene_.release()));
			scene_ = std::move(nextScene);
			input_.Reset();
			lobby = nullptr;
			if (!shooting->SetNetworkSession(*network_, result))
				throw std::runtime_error("방 입장 정보가 올바르지 않습니다.");

			// 통신 스레드는 장면을 만지지 않는다. 메인에서 초기화가 끝난 뒤 로딩을 알린다.
			scene_->Initialize();
			sceneInitialized_ = true;
			previousTime_ = std::chrono::steady_clock::now();
			shooting->SendMapLoaded();
			std::cout << "Room " << result.room.version.context.roomId << " joined ("
				<< result.room.vPlayers.size() << '/' << static_cast<int>(result.room.capacity)
				<< "). Map loaded." << std::endl;
			break;
		}
		case Net::TcpType::LeaveRoomResult:
		{
			Net::LeaveRoomResult result;
			if (!Net::PacketCodec::ReadMessage(header.type, messageBytes, messageSize, result))
				throw std::runtime_error("방 퇴장 응답을 읽지 못했습니다.");
			if (roomMenu_ && roomMenu_->OnRoomExited(header.requestId, result))
			{
				lobby = roomMenu_.get();
				shooting = nullptr;
				scene_ = std::move(roomMenu_);
				input_.Reset();
			}
			break;
		}
		case Net::TcpType::RoomState:
		{
			Net::RoomState state;
			if (shooting && Net::PacketCodec::ReadMessage(header.type, messageBytes, messageSize, state))
				shooting->OnRoomStateChanged(state.room);
			break;
		}
		case Net::TcpType::GameStarted:
		{
			Net::GameStarted event;
			if (shooting && Net::PacketCodec::ReadMessage(header.type, messageBytes, messageSize, event))
				shooting->OnGameStarted(event);
			break;
		}
		case Net::TcpType::MatchTime:
		{
			Net::MatchTime time;
			if (shooting && Net::PacketCodec::ReadMessage(header.type, messageBytes, messageSize, time))
				shooting->UpdateMatchTime(time);
			break;
		}
		case Net::TcpType::GameResults:
		{
			Net::GameResults result;
			if (shooting && Net::PacketCodec::ReadMessage(header.type, messageBytes, messageSize, result))
				shooting->RecvGameResults(result);
			break;
		}
		case Net::TcpType::ShotResolved:
		{
			Net::ShotResolved result;
			if (shooting && Net::PacketCodec::ReadMessage(header.type, messageBytes, messageSize, result))
				shooting->RecvShootResult(result);
			break;
		}
		case Net::TcpType::RequestRejected:
		{
			Net::RequestRejected result;
			if (!Net::PacketCodec::ReadMessage(header.type, messageBytes, messageSize, result))
				break;
			if (roomMenu_ && roomMenu_->OnLeaveRejected(header.requestId))
				break;
			if (shooting)
				shooting->OnReadyRejected(header.requestId);
			if (result.reason == Net::ErrorCode::RoomNotFound)
				throw std::runtime_error("참가한 방이 종료되어 요청을 처리할 수 없습니다.");
			break;
		}
		default:
			break;
		}
	}
	readyPanelActive_ = shooting && shooting->IsPreparing();
	bool showCursor = lobby || (shooting && !shooting->IsPlaying());
	if (menuCursorActive_ != showCursor)
	{
		menuCursorActive_ = showCursor;
		glutSetCursor(showCursor ? GLUT_CURSOR_LEFT_ARROW : GLUT_CURSOR_NONE);
		centerAimOnMotion_ = !showCursor;
	}
}

void Framework::UploadLights(const Shader& shader) const
{
	shader.Set("ambientLight", scene_->ambientLight);
	int count = 0;

	for (const Light& light : scene_->GetLights())
	{
		if (!light.enabled)
		{
			continue;
		}

		if (count == Light::MaxCount)
		{
			break;
		}

		const std::string prefix = "lights[" + std::to_string(count++) + "].";
		shader.Set(prefix + "type", static_cast<int>(light.type));
		shader.Set(prefix + "color", light.color);
		shader.Set(prefix + "intensity", std::max(light.intensity, 0.0f));
		shader.Set(prefix + "position", light.position);
		shader.Set(prefix + "direction", light.direction);
		shader.Set(prefix + "range", std::max(light.range, 0.0f));
		float outer = std::clamp(light.outerAngle, 0.1f, 89.0f);
		float inner = std::clamp(light.innerAngle, 0.0f, outer - 0.1f);
		shader.Set(prefix + "innerCos", std::cos(glm::radians(inner)));
		shader.Set(prefix + "outerCos", std::cos(glm::radians(outer)));
	}

	shader.Set("lightCount", count);
}

void Framework::DrawObjects()
{
	const Camera& camera = scene_->GetCamera();
	glm::mat4 projection = camera.GetProjection(static_cast<float>(width_) / height_);
	glm::mat4 view = camera.GetView();
	const Shader* previousShader = nullptr;

	for (const std::unique_ptr<Object>& object : scene_->GetObjects())
	{
		if (!object->active || !object->visible)
		{
			continue;
		}

		const std::shared_ptr<Mesh>& mesh = object->GetMesh();
		const std::shared_ptr<Material>& material = object->GetMaterial();

		if (!mesh || !material || !material->shader)
		{
			continue;
		}

		if (glm::any(glm::lessThan(glm::abs(object->transform.scale), glm::vec3(0.000001f))))
		{
			throw std::runtime_error("Object scale must be nonzero: " + object->name);
		}

		material->Apply();
		const Shader& shader = *material->shader;

		if (previousShader != &shader)
		{
			shader.Set("view", view);
			shader.Set("projection", projection);
			shader.Set("viewPosition", camera.position);
			UploadLights(shader);
			previousShader = &shader;
		}

		shader.Set("model", object->GetModelMatrix());
		mesh->Draw();
	}
}

//LegacyGL
void Framework::DrawColliders()
{
	if (!scene_->debugColliders) return;

	// Program 0 selects the fixed-function pipeline in this compatibility context.
	//LegacyGL
	glUseProgram(0);
	glDisable(GL_CULL_FACE);
	//LegacyGL
	glMatrixMode(GL_PROJECTION);
	glm::mat4 projection = scene_->GetCamera().GetProjection(static_cast<float>(width_) / height_);
	//LegacyGL
	glLoadMatrixf(glm::value_ptr(projection));
	//LegacyGL
	glMatrixMode(GL_MODELVIEW);
	glm::mat4 view = scene_->GetCamera().GetView();
	//LegacyGL
	glLoadMatrixf(glm::value_ptr(view));
	//LegacyGL
	glColor3f(1, 0, 0);

	for (const std::unique_ptr<Object>& object : scene_->GetObjects())
	{
		if (!object->active || object->collider == ColliderType::None)
		{
			continue;
		}

		//LegacyGL
		glPushMatrix();

		if (object->collider == ColliderType::Box)
		{
			Box box = object->GetWorldBox();
			glm::vec3 center = (box.min + box.max) * 0.5f;
			glm::vec3 size = box.max - box.min;
			//LegacyGL
			glTranslatef(center.x, center.y, center.z);
			//LegacyGL
			glScalef(size.x, size.y, size.z);
			// GLUT drawing helper used with the legacy fixed-function pipeline here.
			//LegacyGL
			glutWireCube(1.0);
		}
		else
		{
			Sphere sphere = object->GetWorldSphere();
			//LegacyGL
			glTranslatef(sphere.center.x, sphere.center.y, sphere.center.z);
			// GLUT drawing helper used with the legacy fixed-function pipeline here.
			//LegacyGL
			glutWireSphere(sphere.radius, 12, 8);
		}

		//LegacyGL
		glPopMatrix();
	}

	glEnable(GL_CULL_FACE);
}

//LegacyGL
void Framework::DrawOverlay()
{
	const Hud hud = scene_->GetHud();
	// Program 0 selects the fixed-function pipeline in this compatibility context.
	//LegacyGL
	glUseProgram(0);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	//LegacyGL
	glMatrixMode(GL_PROJECTION);
	//LegacyGL
	glLoadIdentity();
	//LegacyGL
	glOrtho(-1, 1, -1, 1, -1, 1);
	//LegacyGL
	glMatrixMode(GL_MODELVIEW);
	//LegacyGL
	glLoadIdentity();

	if (!hud.lobbyButtons.empty())
		DrawLobbyPanel(hud);
	else if (!hud.readyButton.empty())
		DrawReadyPanel(hud);
	else if (hud.showResults)
	{
		glColor3f(0.10f, 0.13f, 0.17f);
		glRectf(-0.94f, -0.65f, 0.94f, 0.80f);
		glLineWidth(1.7f);
		glColor3f(0.94f, 0.96f, 0.98f);
		DrawText(hud.message, -0.84f, 0.60f, 0.00065f);
		float y = 0.35f;
		for (const std::string& line : hud.lines)
		{
			float scale = std::min(0.00055f, 1.68f / std::max(1.0f, ReadyTextWidth(line)));
			DrawText(line, -0.84f, y, scale);
			y -= 0.18f;
		}
	}
	else
	{
		if (hud.crosshair)
		{
			//LegacyGL
			glColor3f(1, 1, 1);
			glLineWidth(2.0f);
			//LegacyGL
			glBegin(GL_LINES);
			//LegacyGL
			glVertex2f(0.025f, 0);
			//LegacyGL
			glVertex2f(-0.025f, 0);
			//LegacyGL
			glVertex2f(0, 0.025f);
			//LegacyGL
			glVertex2f(0, -0.025f);
			//LegacyGL
			glEnd();

			if (hud.hitIndicatorAlpha > 0.0f)
			{
				glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT | GL_LINE_BIT);
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glColor4f(1.0f, 1.0f, 1.0f, hud.hitIndicatorAlpha);
				glLineWidth(2.0f);
				float radiusPixels = 0.035f * static_cast<float>(std::max(width_, height_)) * 0.5f;
				float radiusX = 2.0f * radiusPixels / width_;
				float radiusY = 2.0f * radiusPixels / height_;
				constexpr int CircleSegments = 64;
				glBegin(GL_LINE_LOOP);
				for (int index = 0; index < CircleSegments; ++index)
				{
					float angle = glm::radians(360.0f) * index / CircleSegments;
					glVertex2f(std::cos(angle) * radiusX, std::sin(angle) * radiusY);
				}
				glEnd();
				glPopAttrib();
			}
			// 원본은 이후 HUD에도 선 두께 2를 유지합니다.
		}

		//LegacyGL
		glColor3f(0, 0, 0);
		const std::array<glm::vec2, 4> positions{{{-0.99f, 0.9f}, {-0.99f, 0.8f}, {-0.3f, 0.9f}, {0.6f, 0.9f}}};

		for (size_t i = 0; i < hud.lines.size() && i < positions.size(); ++i)
		{
			DrawText(hud.lines[i], positions[i].x, positions[i].y, 0.0005f);
		}

		if (!hud.message.empty())
		{
			DrawText(hud.message, -0.6f, -0.15f, hud.messageScale);
		}
	}

	if (roomMenu_)
	{
		glColor3f(0.14f, 0.18f, 0.22f);
		glRectf(LeaveLeft, LeaveBottom, LeaveRight, LeaveTop);
		glColor3f(0.94f, 0.96f, 0.98f);
		glLineWidth(1.7f);
		DrawText(roomMenu_->LeaveButtonText(), LeaveLeft + 0.04f, LeaveBottom + 0.055f, 0.00046f);
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}

void Framework::Render()
{
	if (!scene_) return;

	glViewport(0, 0, width_, height_);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if (sceneInitialized_)
	{
		DrawObjects();
		DrawColliders();
		DrawOverlay();
	}
	glutSwapBuffers();
}

void Framework::Close()
{
	glutLeaveMainLoop();
}

void Framework::Fail(const std::exception& error)
{
	std::cerr << error.what() << '\n';
	exitCode_ = 1;
	Close();
}

void Framework::DisplayCallback()
{
	try
	{
		current_->Render();
	}
	catch (const std::exception& e)
	{
		current_->Fail(e);
	}
}

void Framework::TimerCallback(int)
{
	if (!current_ || !current_->scene_) return;

	try
	{
		current_->Tick();
	}
	catch (const std::exception& e)
	{
		current_->Fail(e);
	}

	glutTimerFunc(UpdateIntervalMs, TimerCallback, 0);
}

void Framework::ReshapeCallback(int width, int height)
{
	current_->width_ = std::max(width, 1);
	current_->height_ = std::max(height, 1);
}

void Framework::KeyDownCallback(unsigned char key, int, int)
{
	if (key == 'q')
	{
		current_->Close();
		return;
	}

	if (current_->input_.IsDown(key) &&
		((key == ' ' && current_->readyPanelActive_) || key == 'l' || key == 'L'))
		return;
	if (key == ' ' && current_->readyPanelActive_)
		current_->input_.ToggleReady();
	current_->input_.SetKey(key, true);
}

void Framework::KeyUpCallback(unsigned char key, int, int)
{
	current_->input_.SetKey(key, false);
}

void Framework::MouseCallback(int button, int state, int x, int y)
{
	if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;

	if (current_->menuCursorActive_)
	{
		float px = 2.0f * x / current_->width_ - 1.0f;
		float py = 1.0f - 2.0f * y / current_->height_;
		if (MainScene* lobby = dynamic_cast<MainScene*>(current_->scene_.get()))
		{
			try
			{
				if (current_->sceneInitialized_)
					lobby->PickButton(px, py);
			}
			catch (const std::exception& e)
			{
				current_->Fail(e);
			}
			return;
		}
		if (current_->roomMenu_ && px >= LeaveLeft && px <= LeaveRight && py >= LeaveBottom && py <= LeaveTop)
			current_->input_.leaveClicked = true;
		else if (current_->readyPanelActive_ && px >= ReadyLeft && px <= ReadyRight && py >= ReadyBottom && py <= ReadyTop)
			current_->input_.ToggleReady();
		return;
	}
	current_->input_.firePressed = true;
}

void Framework::MotionCallback(int x, int y)
{
	if (current_->menuCursorActive_) return;
	int dx = x - current_->width_ / 2, dy = y - current_->height_ / 2;

	// 대기 화면을 닫고 처음 움직일 때는 버튼과 조준점 사이의 거리를 시선에 적용하지 않는다.
	bool recenter = current_->centerAimOnMotion_;
	current_->centerAimOnMotion_ = false;
	if (dx == 0 && dy == 0) return;

	if (!recenter)
		current_->input_.mouseDelta += glm::vec2(dx, dy);
	glutWarpPointer(current_->width_ / 2, current_->height_ / 2);
}

void Framework::CloseCallback()
{
	// 창 X 버튼과 glutLeaveMainLoop 모두 컨텍스트 삭제 전에 이 콜백을 거칩니다.
	current_->scene_.reset();
	current_->roomMenu_.reset();
	current_->sceneInitialized_ = false;
	current_->window_ = 0;
}
