#pragma once
#include "pch.h"
#include "Scene.h"

// 창, 입력 수집, 시간 루프, 렌더링을 담당합니다. 게임 규칙은 Scene로 전달합니다.
class Framework
{
public:
	int Run(int argc, char** argv, std::unique_ptr<Scene> scene);

private:
	void Tick();
	void Render();
	void DrawObjects();
	void DrawOverlay();
	void DrawColliders();
	void UploadLights(const Shader& shader) const;
	void Close();
	void Fail(const std::exception& error);

	// GLUT C 콜백과 현재 Framework 인스턴스를 연결하는 유일한 전역 진입점입니다.
	static Framework* current_;
	static void DisplayCallback();
	static void TimerCallback(int);
	static void ReshapeCallback(int width, int height);
	static void KeyDownCallback(unsigned char key, int, int);
	static void KeyUpCallback(unsigned char key, int, int);
	static void MouseCallback(int button, int state, int, int);
	static void MotionCallback(int x, int y);
	static void CloseCallback();

	std::unique_ptr<Scene>				  scene_;
	Input								  input_;
	int									  window_ = 0;
	int									  width_ = 800;
	int									  height_ = 600;
	int									  exitCode_ = 0;
	std::chrono::steady_clock::time_point previousTime_;
};
