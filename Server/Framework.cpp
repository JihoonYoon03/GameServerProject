#include "pch.h"
#include "Framework.h"
#include "NetworkServer.h"
#include "RoomManager.h"

Framework::Framework() = default;

Framework::~Framework()
{
    StopServer();
}

bool Framework::StartServer(std::uint16_t tcpPort, std::uint16_t udpPort)
{
    if (network_ != nullptr)
        return false;
    roomManager_ = std::make_unique<RoomManager>();
    network_ = std::make_unique<NetworkServer>();
    network_->SetRoomManager(roomManager_.get());
    if (!network_->StartNetwork(tcpPort, udpPort))
    {
        network_.reset();
        roomManager_.reset();
        return false;
    }
    return true;
}

void Framework::RunServer(const std::function<bool()>& shouldStop)
{
    if (network_ == nullptr || runInProgress_ || stopInProgress_)
        return;

    runInProgress_ = true;
    struct RunGuard
    {
        bool& running;
        ~RunGuard() { running = false; }
    } guard{runInProgress_};
    network_->NetworkLoop(shouldStop);
}

void Framework::SetRoomStopCallback(const std::function<void()>& callback)
{
    // 실행 중인 종료 콜백이 자기 자신을 교체하지 못하게 한다.
    if (!stopInProgress_)
        roomStopCallback_ = callback;
}

void Framework::StopServer()
{
    if (network_ == nullptr || stopInProgress_)
        return;

    if (runInProgress_)
    {
        network_->StopNetwork();
        return;
    }

    stopInProgress_ = true;
    // 방 종료를 먼저 요청하되 실패해도 통신 스레드와 소켓을 정리한다.
    try
    {
        if (roomStopCallback_)
            roomStopCallback_();
    }
    catch (...)
    {
        fputs("Room stop callback failed.\n", stderr);
    }

    network_->StopNetwork();
    network_.reset();
    roomManager_.reset();
    stopInProgress_ = false;
}


namespace
{
    bool StopFromInput()
    {
        const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(input, &mode))
        {
            DWORD pending = 0;
            if (!GetNumberOfConsoleInputEvents(input, &pending))
                return true;

            while (pending-- > 0)
            {
                INPUT_RECORD event{};
                DWORD read = 0;
                if (!ReadConsoleInputW(input, &event, 1, &read))
                    return true;
                if (read != 0 && event.EventType == KEY_EVENT && event.Event.KeyEvent.bKeyDown &&
                    event.Event.KeyEvent.wVirtualKeyCode == VK_RETURN)
                    return true;
            }
            return false;
        }

        char buffer[256];
        DWORD available = sizeof(buffer);
        if (GetFileType(input) == FILE_TYPE_PIPE)
        {
            if (!PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr))
                return true;
            if (available == 0)
                return false;
        }
        DWORD read = 0;
        const DWORD length = (available < sizeof(buffer)) ? available : sizeof(buffer);
        return !ReadFile(input, buffer, length, &read, nullptr) || read == 0 ||
            memchr(buffer, '\n', read) != nullptr || memchr(buffer, '\r', read) != nullptr;
    }

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
    framework.RunServer(StopFromInput);
    framework.StopServer();
    return 0;
}
