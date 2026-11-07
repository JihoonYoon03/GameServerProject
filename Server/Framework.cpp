#include "pch.h"
#include "Framework.h"
#include "NetworkServer.h"
#include "RoomManager.h"

Framework::Framework() = default;

Framework::~Framework()
{
    StopServer();
}

// 이성노 수정 필요: Room::SendTargetPositions의 이전 Client 필드 참조를 현재 인터페이스에 맞춘다.
// 윤지훈 협의 필요: Room에서 조회할 방 번호와 멤버십 번호의 접근 인터페이스를 제공한다.
// 이성노 후속 일정: 11/12 표적 송신을 버전 포함 44바이트 공용 코덱과 송신 큐로 이관한다.
bool Framework::StartServer(std::uint16_t tcpPort, std::uint16_t udpPort)
{
    if (network_ != nullptr)
        return false;
    roomManager_ = std::make_unique<RoomManager>();
    network_ = std::make_unique<NetworkServer>();
    network_->SetRoomManager(roomManager_.get());
    if (!network_->StartNetwork(tcpPort, udpPort))
    {
        StopServer();
        return false;
    }
    return true;
}

void Framework::StopServer()
{
    if (network_ != nullptr)
        network_->StopNetwork();
    network_.reset();
    roomManager_.reset();
}


#ifndef SERVER_NETWORK_TEST
namespace
{
    bool ReadPort(const char* text, std::uint16_t& port)
    {
        unsigned int value = 0;
        if (*text == '\0')
            return false;
        for (const char* current = text; *current != '\0'; ++current)
        {
            if (*current < '0' || *current > '9')
                return false;
            value = value * 10 + (*current - '0');
            if (value > 65535)
                return false;
        }
        if (value == 0)
            return false;
        port = static_cast<std::uint16_t>(value);
        return true;
    }
}

int main(int argc, char* argv[])
{
    std::uint16_t tcpPort = 0;
    std::uint16_t udpPort = 0;
    if (argc != 3 || !ReadPort(argv[1], tcpPort) || !ReadPort(argv[2], udpPort))
    {
        printf("Usage: Server.exe TCP_PORT UDP_PORT (1-65535)\n");
        return 1;
    }
    Framework framework;
    if (!framework.StartServer(tcpPort, udpPort))
    {
        printf("Server start failed.\n");
        return 1;
    }
    printf("Server started: TCP %u / UDP %u. Press Enter to stop.\n", tcpPort, udpPort);
    getchar();
    framework.StopServer();
    return 0;
}
#endif
