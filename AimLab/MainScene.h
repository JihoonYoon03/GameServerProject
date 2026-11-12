#pragma once
#include "Scene.h"
#include "Shared/NetworkPackets.h"

class MainScene : public Scene
{
public:
    void Initialize() override;
    void Update(float deltaTime, const Input& input) override;
    void RequestGameRoom();
    void JoinRoom(const Net::RoomEnterResult& result);
    void ExitRoom();
    void OnRoomExited(const Net::LeaveRoomResult& result);
    void PickButton(int mouseX, int mouseY);
    void DrawText(const std::string& text, float x, float y);
};
