#include "pch.h"
#include "Framework.h"
#include "NetworkClient.h"
#include "ShootingScene.h"
#include "../Shared/PacketCodec.h"

Framework* Framework::current_ = nullptr;

namespace
{
	constexpr int UpdateIntervalMs = 1000 / 240;

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
		glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
		glutInitWindowSize(width_, height_);
		glutInitWindowPosition(100, 100);
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
	ShootingScene* shooting = dynamic_cast<ShootingScene*>(scene_.get());
	if (shooting == nullptr)
		throw std::runtime_error("1인 접속에 필요한 사격장 장면이 없습니다.");

	for (const Net::PacketData& packet : network_->GetReceivedPackets())
	{
		if (packet.transport == Net::PacketTransport::Udp)
		{
			Net::UdpHeader header;
			Net::PlayerSnapshot snapshot;

			//헤더를 읽은 뒤, 타입 판단 후 Read. 성공 시 변환 데이터 적용
			if (sceneInitialized_ && Net::PacketCodec::ReadUdpHeader(packet.bytes.data(), packet.bytes.size(), header)
				&& header.type == Net::UdpType::PlayerSnapshot
				&& Net::PacketCodec::ReadMessage(header.type, packet.bytes.data() + Net::UdpHeaderBytes,
					packet.bytes.size() - Net::UdpHeaderBytes, snapshot))
				shooting->RecvTransformData(snapshot);

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
		case Net::TcpType::RoomEnterResult:
		{
			Net::RoomEnterResult result;
			if (!Net::PacketCodec::ReadMessage(header.type, messageBytes, messageSize, result))
				throw std::runtime_error("방 입장 응답을 읽지 못했습니다.");
			if (result.result != Net::ErrorCode::Ok)
				throw std::runtime_error("방 입장 실패: " + std::to_string(static_cast<unsigned int>(result.result)));
			if (sceneInitialized_)
				break;
			if (!shooting->SetNetworkSession(*network_, result))
				throw std::runtime_error("방 입장 정보가 올바르지 않습니다.");

			// 통신 스레드는 장면을 만지지 않는다. 메인에서 초기화가 끝난 뒤 로딩을 알린다.
			scene_->Initialize();
			sceneInitialized_ = true;
			previousTime_ = std::chrono::steady_clock::now();
			shooting->SendMapLoaded();
			std::cout << "1인 방 입장 및 맵 로딩 완료: " << result.room.version.context.roomId << '\n';
			break;
		}
		case Net::TcpType::RoomState:
		{
			Net::RoomState state;
			if (sceneInitialized_ && Net::PacketCodec::ReadMessage(header.type, messageBytes, messageSize, state))
				shooting->OnRoomStateChanged(state.room);
			break;
		}
		case Net::TcpType::GameStarted:
		{
			Net::GameStarted event;
			if (sceneInitialized_ && Net::PacketCodec::ReadMessage(header.type, messageBytes, messageSize, event))
				shooting->OnGameStarted(event);
			break;
		}
		case Net::TcpType::ShotResolved:
		{
			Net::ShotResolved result;
			if (sceneInitialized_ && Net::PacketCodec::ReadMessage(header.type, messageBytes, messageSize, result))
				shooting->RecvShootResult(result);
			break;
		}
		case Net::TcpType::RequestRejected:
		{
			Net::RequestRejected result;
			if (Net::PacketCodec::ReadMessage(header.type, messageBytes, messageSize, result)
				&& result.reason == Net::ErrorCode::RoomNotFound)
				throw std::runtime_error("참가한 방이 종료되어 요청을 처리할 수 없습니다.");
			break;
		}
		default:
			// 윤지훈·이성노 연결 필요: 사격 응답·표적·점수·시간·결과를 각 담당 함수에 반영한다.
			break;
		}
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

	current_->input_.SetKey(key, true);
}

void Framework::KeyUpCallback(unsigned char key, int, int)
{
	current_->input_.SetKey(key, false);
}

void Framework::MouseCallback(int button, int state, int, int)
{
	if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;

	current_->input_.firePressed = true;
}

void Framework::MotionCallback(int x, int y)
{
	int dx = x - current_->width_ / 2, dy = y - current_->height_ / 2;

	if (dx == 0 && dy == 0) return;

	current_->input_.mouseDelta += glm::vec2(dx, dy);
	glutWarpPointer(current_->width_ / 2, current_->height_ / 2);
}

void Framework::CloseCallback()
{
	// 창 X 버튼과 glutLeaveMainLoop 모두 컨텍스트 삭제 전에 이 콜백을 거칩니다.
	current_->scene_.reset();
	current_->sceneInitialized_ = false;
	current_->window_ = 0;
}
