#pragma once
#include "Scene.h"
#include "Shared/NetworkPackets.h"

class NetworkClient;

class MainScene : public Scene
{
public:
    explicit MainScene(NetworkClient& network);
    void Initialize() override;
    void Update(float deltaTime, const Input& input) override;
    Hud GetHud() const override;
    void SearchRoomList();
    void UpdateRoomList(std::uint32_t requestId, const Net::RoomListResult& result);
    bool JoinRoom(const Net::RoomEnterResult& result);
    void ExitRoom();
    void OnRoomExited(const Net::LeaveRoomResult& result);
    void PickButton(int mouseX, int mouseY);
    void DrawText(const std::string& text, float x, float y);

private:
    void ShowRoomList() const;
    void RequestEntry(Net::RoomId roomId);
    NetworkClient* network_;
    std::vector<Net::RoomInfo> roomList_;
    std::uint32_t pendingListRequestId_ = 0;
    std::size_t selected_ = 0;
    bool firstList_ = true;
    bool entering_ = false;
    std::string notice_;
};
