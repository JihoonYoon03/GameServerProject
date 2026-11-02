#pragma once
#include <cstdint>

namespace Server
{
    class Framework
    {
    public:
        bool StartServer(std::uint16_t tcpPort, std::uint16_t udpPort);
        void StopServer();
    };
}
