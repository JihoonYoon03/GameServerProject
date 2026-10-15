#include "pch.h"
#include "Framework.h"

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
		scene_->Initialize();

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

	if (window_ > 0)
	{
		const int window = window_;
		window_ = 0;
		glutDestroyWindow(window);
	}

	current_ = nullptr;

	return exitCode_;
}

void Framework::Tick()
{
	const auto now = std::chrono::steady_clock::now();
	const float elapsed = std::chrono::duration<float>(now - previousTime_).count();
	previousTime_ = now;

	scene_->Update(elapsed, input_);
	input_.EndFrame();

	if (elapsed > 0.0f)
	{
		glutSetWindowTitle(("FPS: " + std::to_string(static_cast<int>(1.0f / elapsed))).c_str());
	}

	glutPostRedisplay();
}

void Framework::UploadLights(const Shader& shader) const
{
	shader.Set("ambientLight", scene_->ambientLight);
	int count = 0;

	for (const auto& light : scene_->GetLights())
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
		const float outer = std::clamp(light.outerAngle, 0.1f, 89.0f);
		const float inner = std::clamp(light.innerAngle, 0.0f, outer - 0.1f);
		shader.Set(prefix + "innerCos", std::cos(glm::radians(inner)));
		shader.Set(prefix + "outerCos", std::cos(glm::radians(outer)));
	}

	shader.Set("lightCount", count);
}

void Framework::DrawObjects()
{
	const auto& camera = scene_->GetCamera();
	const glm::mat4 projection = camera.GetProjection(static_cast<float>(width_) / height_);
	const glm::mat4 view = camera.GetView();
	const Shader* previousShader = nullptr;

	for (const auto& object : scene_->GetObjects())
	{
		if (!object->active || !object->visible)
		{
			continue;
		}

		const auto& mesh = object->GetMesh();
		const auto& material = object->GetMaterial();

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
	const auto projection = scene_->GetCamera().GetProjection(static_cast<float>(width_) / height_);
	//LegacyGL
	glLoadMatrixf(glm::value_ptr(projection));
	//LegacyGL
	glMatrixMode(GL_MODELVIEW);
	const auto view = scene_->GetCamera().GetView();
	//LegacyGL
	glLoadMatrixf(glm::value_ptr(view));
	//LegacyGL
	glColor3f(1, 0, 0);

	for (const auto& object : scene_->GetObjects())
	{
		if (!object->active || object->collider == ColliderType::None)
		{
			continue;
		}

		//LegacyGL
		glPushMatrix();

		if (object->collider == ColliderType::Box)
		{
			const auto box = object->GetWorldBox();
			const auto center = (box.min + box.max) * 0.5f;
			const auto size = box.max - box.min;
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
			const auto sphere = object->GetWorldSphere();
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
	DrawObjects();
	DrawColliders();
	DrawOverlay();
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
	const int dx = x - current_->width_ / 2, dy = y - current_->height_ / 2;

	if (dx == 0 && dy == 0) return;

	current_->input_.mouseDelta += glm::vec2(dx, dy);
	glutWarpPointer(current_->width_ / 2, current_->height_ / 2);
}

void Framework::CloseCallback()
{
	// 창 X 버튼과 glutLeaveMainLoop 모두 컨텍스트 삭제 전에 이 콜백을 거칩니다.
	current_->scene_.reset();
	current_->window_ = 0;
}
