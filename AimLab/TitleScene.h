#pragma once
#include "Scene.h"

class TitleScene : public Scene
{
public:
	explicit TitleScene(std::string serverAddress);
	void Initialize() override;
	void Update(float deltaTime, const Input& input) override;
	Hud GetHud() const override;
	void HandleKey(unsigned char key);
	void SetConnecting(bool connecting);
	void SetNotice(const std::string& notice);
	const std::string& GetServerAddress() const { return serverAddress_; }
	bool IsConnecting() const { return connecting_; }

private:
	std::string serverAddress_;
	std::string notice_;
	float blinkTime_ = 0.0f;
	bool connecting_ = false;
};
