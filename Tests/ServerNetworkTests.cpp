#include "../Server/pch.h"
#include "../Server/NetworkServer.h"
#include "../Server/Framework.h"
#include "../Shared/PacketCodec.h"
#include <cstdio>
#include <cstdlib>

// 부분 송신과 송신 대기는 운영체제 버퍼 크기에 의존하지 않도록 대역으로 재현한다.
namespace
{
    bool useSendFixture = false;
    bool probeFatalSend = false;
    int sendStep = 0;
    std::vector<std::uint8_t> fixtureBytes;
    int FixtureSend(SOCKET socketHandle, const char* bytes, int length, int flags)
    {
        if (!useSendFixture)
            return send(socketHandle, bytes, length, flags);
        ++sendStep;
        // 진단 모드에서는 연결 오류 뒤 송신 위치가 되돌아가는지 별도 프로세스로 관찰한다.
        if (probeFatalSend && sendStep == 3)
        {
            if (length == 6 && static_cast<unsigned char>(bytes[0]) == 3)
            {
                std::printf("REPRODUCED: fatal send error rewinds offset from 3 to 2.\n");
                std::exit(42);
            }
            std::exit(43);
        }
        if (sendStep == 2)
        {
            WSASetLastError(probeFatalSend ? WSAECONNRESET : WSAEWOULDBLOCK);
            return SOCKET_ERROR;
        }
        int sent = length;
        if (sendStep == 1 && sent > 3)
            sent = 3;
        fixtureBytes.insert(fixtureBytes.end(), bytes, bytes + sent);
        return sent;
    }
}
#define send FixtureSend
#include "../Server/Client.cpp"
#undef send

namespace
{
    int checks = 0;
    void Check(bool condition, const char* name)
    {
        ++checks;
        if (!condition)
        {
            std::printf("FAIL: %s (%d)\n", name, WSAGetLastError());
            std::exit(1);
        }
    }

    std::uint16_t FreePort()
    {
        SOCKET socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        Check(socketHandle != INVALID_SOCKET, "port socket");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        Check(bind(socketHandle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "port bind");
        int size = sizeof(address);
        Check(getsockname(socketHandle, reinterpret_cast<sockaddr*>(&address), &size) == 0, "port address");
        closesocket(socketHandle);
        return ntohs(address.sin_port);
    }

    SOCKET Connect(std::uint16_t port)
    {
        SOCKET socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        Check(socketHandle != INVALID_SOCKET, "peer socket");
        DWORD timeout = 3000;
        setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        Check(connect(socketHandle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "peer connect");
        return socketHandle;
    }

    std::vector<std::uint8_t> Receive(SOCKET socketHandle, std::size_t length)
    {
        std::vector<std::uint8_t> bytes(length);
        std::size_t offset = 0;
        while (offset < length)
        {
            int received = recv(socketHandle, reinterpret_cast<char*>(bytes.data() + offset),
                static_cast<int>(length - offset), 0);
            Check(received > 0, "receive bytes");
            offset += received;
        }
        return bytes;
    }

    void TestPartialSend()
    {
        Client client;
        SOCKET socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        Check(socketHandle != INVALID_SOCKET, "fixture socket");
        client.SetConnection(socketHandle, Net::SessionToken{});
        Net::PacketData packet;
        packet.bytes = {1, 2, 3, 4, 5, 6, 7, 8};
        client.QueueSend(packet);
        client.QueueSend(packet);
        useSendFixture = true;
        sendStep = 0;
        fixtureBytes.clear();
        NetworkServer server;
        server.SendTcpPending(client);
        Check(client.HasPendingSend(), "partial send retains remainder");
        Check(fixtureBytes == std::vector<std::uint8_t>({1, 2, 3}), "partial prefix");
        server.SendTcpPending(client);
        Check(!client.HasPendingSend(), "partial queue drained");
        std::vector<std::uint8_t> expected = packet.bytes;
        expected.insert(expected.end(), packet.bytes.begin(), packet.bytes.end());
        Check(fixtureBytes == expected, "partial send preserves bytes and order");
        useSendFixture = false;
    }

    bool RecordRoomRequest(NetworkServer& server, std::vector<Net::SendRequest>& records,
        const Net::SendRequest& request)
    {
        records.push_back(request);
        return server.SendPacket(request);
    }

    struct Producer
    {
        NetworkServer* server;
        Net::SessionToken token;
        std::uint8_t id;
        bool succeeded = true;
    };

    DWORD WINAPI Produce(LPVOID parameter)
    {
        Producer* producer = static_cast<Producer*>(parameter);
        for (int i = 0; i < 200; ++i)
        {
            Net::SendRequest request;
            request.recipientToken = producer->token;
            request.packet.bytes = {producer->id, static_cast<std::uint8_t>(i)};
            if (!producer->server->SendPacket(request))
                producer->succeeded = false;
        }
        return 0;
    }

    void TestStartFailure()
    {
        SOCKET occupied = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        Check(occupied != INVALID_SOCKET, "occupied socket");
        BOOL exclusive = TRUE;
        Check(setsockopt(occupied, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
            reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == 0, "exclusive port");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        Check(bind(occupied, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "occupy bind");
        Check(listen(occupied, 1) == 0, "occupy listen");
        int size = sizeof(address);
        Check(getsockname(occupied, reinterpret_cast<sockaddr*>(&address), &size) == 0, "occupied port");
        NetworkServer server;
        Check(!server.StartNetwork(ntohs(address.sin_port), 0), "occupied port rejected");
        closesocket(occupied);
        Check(server.StartNetwork(ntohs(address.sin_port), 0), "recover from start failure");
        server.StopNetwork();
        DWORD before = 0;
        DWORD after = 0;
        Check(GetProcessHandleCount(GetCurrentProcess(), &before) != 0, "handle baseline");
        for (int i = 0; i < 30; ++i)
        {
            Check(server.StartNetwork(0, 0), "repeated start");
            server.StopNetwork();
        }
        Check(GetProcessHandleCount(GetCurrentProcess(), &after) != 0, "handle count");
        Check(after <= before, "no handle growth over 30 restarts");
    }

    void TestUdpFailureAndShutdown()
    {
        SOCKET occupied = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        Check(occupied != INVALID_SOCKET, "occupied UDP socket");
        BOOL exclusive = TRUE;
        Check(setsockopt(occupied, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
            reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == 0, "exclusive UDP port");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        Check(bind(occupied, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "occupy UDP bind");
        int size = sizeof(address);
        Check(getsockname(occupied, reinterpret_cast<sockaddr*>(&address), &size) == 0, "occupied UDP address");
        const std::uint16_t tcpPort = FreePort();
        Framework framework;
        Check(!framework.StartServer(tcpPort, ntohs(address.sin_port)), "reject occupied UDP port");
        closesocket(occupied);
        // UDP 초기화에 실패했을 때 먼저 만든 TCP 소켓까지 해제되어야 한다.
        Check(framework.StartServer(tcpPort, ntohs(address.sin_port)), "TCP and UDP resources released after failure");
        framework.StopServer();

        SOCKET peer;
        {
            Framework owned;
            Check(owned.StartServer(tcpPort, 0), "destructor test start");
            peer = Connect(tcpPort);
            Receive(peer, 42);
        }
        char byte;
        Check(recv(peer, &byte, 1, 0) == 0, "destructor closes peer");
        closesocket(peer);

        NetworkServer server;
        Check(server.StartNetwork(tcpPort, 0), "shutdown race start");
        Producer producers[4];
        HANDLE threads[4];
        for (int i = 0; i < 4; ++i)
        {
            producers[i] = {&server, Net::SessionToken{}, static_cast<std::uint8_t>(i)};
            threads[i] = CreateThread(nullptr, 0, Produce, &producers[i], 0, nullptr);
            Check(threads[i] != nullptr, "shutdown race producer");
        }
        // 연결되지 않은 수신자의 요청으로 송신 큐와 종료 잠금의 경합만 검사한다.
        server.StopNetwork();
        Check(WaitForMultipleObjects(4, threads, TRUE, 5000) == WAIT_OBJECT_0, "shutdown producers finish");
        for (HANDLE thread : threads)
            CloseHandle(thread);
        Net::SendRequest request;
        request.packet.bytes = {1};
        Check(!server.SendPacket(request), "shutdown rejects remaining work");
        Check(server.StartNetwork(tcpPort, 0), "restart after concurrent shutdown");
        server.StopNetwork();
    }

    void TestServer()
    {
        NetworkServer server;
        Net::SendRequest request;
        request.packet.bytes = {1, 2, 3};
        Check(!server.SendPacket(request), "reject before start");
        const std::uint16_t port = FreePort();
        Check(server.StartNetwork(port, 0), "start network");
        Check(!server.StartNetwork(port, 0), "reject duplicate start");
        SOCKET peers[4];
        Net::SessionToken tokens[4];
        for (int i = 0; i < 4; ++i)
        {
            peers[i] = Connect(port);
            // 접속 응답은 12바이트 헤더와 30바이트 본문으로 구성된다.
            std::vector<std::uint8_t> bytes = Receive(peers[i], 42);
            Net::TcpPacket<Net::ConnectResult> packet;
            std::size_t consumed = 0;
            Check(Net::PacketCodec::DecodeTcp(bytes.data(), bytes.size(), packet, consumed) == Net::DecodeStatus::Complete,
                "decode connect response");
            Check(consumed == bytes.size(), "connect frame size");
            Check(packet.packetData.result == Net::ErrorCode::Ok, "connect result");
            Check(packet.packetData.udpPort != 0, "bound UDP port");
            Check(packet.packetData.tickRate == 30, "tick rate");
            tokens[i] = packet.packetData.sessionToken;
            Check(tokens[i] != Net::SessionToken{}, "nonzero token");
            for (int j = 0; j < i; ++j)
                Check(tokens[i] != tokens[j], "unique tokens");
        }
        // 실제 방 대신 송신 요청을 기록하고 서버 큐부터 실제 수신까지 검사한다.
        std::vector<Net::SendRequest> roomRequests;
        Net::TcpPacket<Net::MatchTime> roomPacket;
        roomPacket.header.type = Net::TcpType::MatchTime;
        roomPacket.header.size = Net::TcpHeaderBytes + 8 + 8 + 8;
        roomPacket.packetData.context = {7, 2};
        roomPacket.packetData.serverTimeMs = 1000;
        roomPacket.packetData.endsAtMs = 121000;
        request.recipientToken = tokens[2];
        Check(Net::PacketCodec::EncodeTcp(roomPacket, request.packet), "encode room send fixture");
        Check(RecordRoomRequest(server, roomRequests, request), "record room send request");
        Check(roomRequests.size() == 1 && roomRequests[0].recipientToken == tokens[2], "record intended room recipient");
        std::vector<std::uint8_t> roomBytes = Receive(peers[2], roomPacket.header.size);
        Net::TcpPacket<Net::MatchTime> receivedRoomPacket;
        std::size_t consumed = 0;
        Check(Net::PacketCodec::DecodeTcp(roomBytes.data(), roomBytes.size(), receivedRoomPacket, consumed)
            == Net::DecodeStatus::Complete, "decode actual room send fixture");
        Check(consumed == roomBytes.size() && receivedRoomPacket.packetData.context.roomId == 7
            && receivedRoomPacket.packetData.context.matchId == 2
            && receivedRoomPacket.packetData.serverTimeMs == 1000
            && receivedRoomPacket.packetData.endsAtMs == 121000, "room request reaches intended peer unchanged");
        // 모르는 토큰의 요청이 다른 접속자에게 전달되지 않는지 확인한다.
        request.recipientToken.fill(255);
        request.packet.bytes = {99};
        Check(server.SendPacket(request), "enqueue unknown recipient");
        for (int i = 3; i >= 0; --i)
        {
            request.recipientToken = tokens[i];
            request.packet.bytes = {static_cast<std::uint8_t>(i), 5, 7};
            Check(server.SendPacket(request), "enqueue recipient");
            request.packet.bytes = {8, 9};
            Check(server.SendPacket(request), "enqueue second packet");
        }
        for (int i = 0; i < 4; ++i)
        {
            std::vector<std::uint8_t> expected{static_cast<std::uint8_t>(i), 5, 7, 8, 9};
            Check(Receive(peers[i], expected.size()) == expected, "recipient and queue order");
        }
        request.packet.bytes.clear();
        Check(!server.SendPacket(request), "empty packet rejected");
        // UDP 등록 전 요청은 TCP 연결로 잘못 보내지 않아야 한다.
        request.recipientToken = tokens[0];
        request.packet.transport = Net::PacketTransport::Udp;
        request.packet.bytes = {99};
        Check(server.SendPacket(request), "unregistered UDP queued");
        request.packet.transport = Net::PacketTransport::Tcp;
        request.packet.bytes = {42};
        Check(server.SendPacket(request), "TCP marker after UDP");
        Check(Receive(peers[0], 1) == std::vector<std::uint8_t>({42}), "UDP never leaks to TCP");
        // 서로 다른 네 스레드가 송신 큐에 동시에 요청을 넣는다.
        Producer producers[4];
        HANDLE threads[4];
        for (int i = 0; i < 4; ++i)
        {
            producers[i] = {&server, tokens[i], static_cast<std::uint8_t>(i)};
            threads[i] = CreateThread(nullptr, 0, Produce, &producers[i], 0, nullptr);
            Check(threads[i] != nullptr, "producer thread");
        }
        Check(WaitForMultipleObjects(4, threads, TRUE, 5000) == WAIT_OBJECT_0, "producers completed");
        for (int i = 0; i < 4; ++i)
        {
            CloseHandle(threads[i]);
            Check(producers[i].succeeded, "all concurrent requests accepted");
            std::vector<std::uint8_t> expected;
            for (int j = 0; j < 200; ++j)
            {
                expected.push_back(static_cast<std::uint8_t>(i));
                expected.push_back(static_cast<std::uint8_t>(j));
            }
            Check(Receive(peers[i], expected.size()) == expected, "800 concurrent packets preserve recipient and order");
        }
        server.StopNetwork();
        Check(!server.SendPacket(request), "reject after stop");
        for (SOCKET peer : peers)
        {
            char byte;
            Check(recv(peer, &byte, 1, 0) == 0, "server closes connection");
            closesocket(peer);
        }
        server.StopNetwork();
        Check(server.StartNetwork(port, 0), "restart network");
        server.StopNetwork();

        Framework framework;
        Check(framework.StartServer(port, 0), "framework start");
        Check(!framework.StartServer(port, 0), "framework duplicate start");
        framework.StopServer();
        Check(framework.StartServer(port, 0), "framework restart");
        framework.StopServer();
    }
}

int main(int argc, char* argv[])
{
    WSADATA data{};
    Check(WSAStartup(MAKEWORD(2, 2), &data) == 0, "test winsock");
    if (argc == 2 && std::strcmp(argv[1], "--probe-fatal-send") == 0)
    {
        probeFatalSend = true;
        TestPartialSend();
        return 44;
    }
    TestServer();
    TestPartialSend();
    TestStartFailure();
    TestUdpFailureAndShutdown();
    WSACleanup();
    std::printf("PASS: server network, %d checks\n", checks);
    return 0;
}
