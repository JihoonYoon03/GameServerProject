#include "pch.h"
#include "TitleScene.h"

TitleScene::TitleScene(std::string serverAddress) : serverAddress_(std::move(serverAddress))
{
}

void TitleScene::Initialize()
{
	blinkTime_ = 0.0f;
}

void TitleScene::Update(float deltaTime, const Input&)
{
	blinkTime_ = std::fmod(blinkTime_ + std::max(deltaTime, 0.0f), 1.2f);
}

Hud TitleScene::GetHud() const
{
	Hud hud;
	hud.crosshair = false;
	hud.showTitle = true;
	hud.titlePromptVisible = connecting_ || blinkTime_ < 0.6f;
	hud.titlePrompt = connecting_ ? "Connecting..." : "Tab Space To Start";
	hud.lines.push_back("Server IP: " + serverAddress_);
	hud.message = notice_;
	return hud;
}

void TitleScene::HandleKey(unsigned char key)
{
	if (connecting_) return;

	if (key == '\b' && !serverAddress_.empty())
		serverAddress_.pop_back();
	else if (((key >= '0' && key <= '9') || key == '.') && serverAddress_.size() < 15)
		serverAddress_.push_back(static_cast<char>(key));
}

void TitleScene::SetConnecting(bool connecting)
{
	connecting_ = connecting;
	blinkTime_ = 0.0f;
	if (connecting)
		notice_.clear();
}

void TitleScene::SetNotice(const std::string& notice)
{
	connecting_ = false;
	notice_ = notice;
}
