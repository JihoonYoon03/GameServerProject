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
    void RunServer(const std::function<bool()>& shouldStop);
    void StopServer();

private:
    std::unique_ptr<NetworkServer> network_;
    std::unique_ptr<RoomManager> roomManager_;
    bool runInProgress_ = false;
    bool stopInProgress_ = false;
};
