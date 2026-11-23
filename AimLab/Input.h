#pragma once
#include "pch.h"

// Framework가 채우고 Scene가 읽습니다. pressed와 mouseDelta는 업데이트 후 초기화합니다.
class Input
{
public:
	void SetKey(unsigned char key, bool down)
	{
		if (down)
		{
			pressed_[key] = true;
		}

		keys_[key] = down;
	}

	bool IsDown(unsigned char key) const { return keys_[key]; }
	bool WasPressed(unsigned char key) const { return pressed_[key]; }

	void ToggleReady()
	{
		readyToggled = !readyToggled;
	}

	void EndFrame()
	{
		pressed_.fill(false);
		mouseDelta = glm::vec2(0.0f);
		firePressed = false;
		readyToggled = false;
		leaveClicked = false;
	}

	void Reset()
	{
		keys_.fill(false);
		EndFrame();
	}

	glm::vec2 mouseDelta{0.0f};
	bool	  firePressed = false;
	bool	  readyToggled = false;
	bool	  leaveClicked = false;

private:
	std::array<bool, 256> keys_{};
	std::array<bool, 256> pressed_{};
};
