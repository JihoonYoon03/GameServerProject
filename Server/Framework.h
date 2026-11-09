#pragma once
#include <cstdint>
#include <functional>
#include <memory>

class NetworkServer;
class RoomManager;

class Framework
{
public:
    Framework();
    ~Framework();
    bool StartServer(std::uint16_t tcpPort, std::uint16_t udpPort);
    void StopServer();
    // 제어 스레드에서 설정한다. 통신 종료 전에 방 종료 대역을 호출한다.
    void SetRoomStopCallback(const std::function<void()>& callback);

private:
    std::unique_ptr<NetworkServer> network_;
    std::unique_ptr<RoomManager> roomManager_;
    std::function<void()> roomStopCallback_;
    bool stopInProgress_ = false;
};
