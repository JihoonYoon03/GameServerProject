#pragma once
#include "Scene.h"
#include "Shared/NetworkPackets.h"

class MainScene : public Scene
{
public:
    void Initialize() override;
    void Update(float deltaTime, const Input& input) override;
    void ShowRoomList();
    void SearchRoomList(Net::RoomId afterRoomId = 0);
    void UpdateRoomList(const Net::RoomListResult& result);
    void RequestGameRoom();
    // 방 ID를 받으면 입장 요청, 입장 결과를 받으면 승인·실패를 처리한다.
    void JoinRoom(Net::RoomId roomId);
    void JoinRoom(const Net::RoomEnterResult& result);
    void ExitRoom();
    void OnRoomExited(const Net::LeaveRoomResult& result);
    void PickButton(int mouseX, int mouseY);
    void DrawText(const std::string& text, float x, float y);
};
