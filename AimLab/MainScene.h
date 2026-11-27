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
    bool OnRoomExited(std::uint32_t requestId, const Net::LeaveRoomResult& result);
    bool OnLeaveRejected(std::uint32_t requestId);
    bool IsLeaving() const;
    const char* LeaveButtonText() const;
    void PickButton(float x, float y);
    void DrawText(const std::string& text, float x, float y);

private:
    void SearchRoomList(bool manual);
    void ShowRoomList() const;
    void SelectRoom(std::size_t index);
    bool CanJoinSelectedRoom() const;
    void RequestEntry(Net::RoomId roomId);
    NetworkClient* network_;
    std::vector<Net::RoomInfo> roomList_;
    std::uint32_t pendingListRequestId_ = 0;
    float autoRefreshElapsed_ = 0.0f;
    bool autoRefreshPending_ = false;
    Net::RoomId roomId_ = 0;
    std::uint32_t pendingLeaveRequestId_ = 0;
    bool leaveFailed_ = false;
    std::size_t selected_ = 0;
    bool firstList_ = true;
    bool entering_ = false;
    std::string notice_;
};
