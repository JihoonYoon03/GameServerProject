#include "../Server/pch.h"
#include "../Server/NetworkServer.h"
#include "../Server/Framework.h"
#include "../Shared/PacketCodec.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

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

// 방 관리자가 연결되기 전에는 시작 전에 정해진 접속 상태를 넣는다.
class NetworkServerTestAccess
{
public:
    static void AddClient(NetworkServer& server, SOCKET socketHandle, const Net::SessionToken& token,
        Net::MatchContext context, Net::MembershipId membershipId)
    {
        Client client;
        client.SetConnection(socketHandle, token);
        client.SetMatchContext(context, membershipId);
        server.m_vClients.push_back(std::move(client));
    }

    static std::uint16_t UdpPort(const NetworkServer& server)
    {
        return server.m_UDPPort;
    }
};

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

    void SendBytes(SOCKET socketHandle, const std::uint8_t* bytes, std::size_t size)
    {
        std::size_t offset = 0;
        while (offset < size)
        {
            const int sent = send(socketHandle, reinterpret_cast<const char*>(bytes + offset),
                static_cast<int>(size - offset), 0);
            Check(sent > 0, "send fixture bytes");
            offset += static_cast<std::size_t>(sent);
        }
    }

    SOCKET ConnectFixture(SOCKET& accepted)
    {
        const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        Check(listener != INVALID_SOCKET, "fixture listener");
        sockaddr_in tcpAddress{};
        tcpAddress.sin_family = AF_INET;
        tcpAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        int addressSize = sizeof(tcpAddress);
        Check(bind(listener, reinterpret_cast<sockaddr*>(&tcpAddress), addressSize) == 0 &&
            listen(listener, 1) == 0 &&
            getsockname(listener, reinterpret_cast<sockaddr*>(&tcpAddress), &addressSize) == 0,
            "bind fixture listener");
        const SOCKET peer = Connect(ntohs(tcpAddress.sin_port));
        accepted = accept(listener, nullptr, nullptr);
        closesocket(listener);
        u_long nonblocking = 1;
        Check(accepted != INVALID_SOCKET && ioctlsocket(accepted, FIONBIO, &nonblocking) == 0,
            "nonblocking fixture connection");
        return peer;
    }

    SOCKET ConnectUdp(std::uint16_t port)
    {
        const SOCKET udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        Check(udp != INVALID_SOCKET, "create UDP fixture peer");
        const DWORD timeout = 3000;
        Check(setsockopt(udp, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
            sizeof(timeout)) == 0, "UDP fixture timeout");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        Check(connect(udp, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0,
            "connect UDP fixture peer");
        return udp;
    }

    void TestReceiveAndDispatch()
    {
        SOCKET accepted = INVALID_SOCKET;
        const SOCKET peer = ConnectFixture(accepted);
        Net::SessionToken token{};
        token[0] = 1;
        const Net::MatchContext context{7, 3};
        const Net::MembershipId membershipId = 11;
        const HANDLE movementReceived = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        Check(movementReceived != nullptr, "create movement event");
        Net::PacketData nestedRequest;
        Check(Net::PacketCodec::EncodeTcp(Net::MapLoaded{context.roomId}, 99, nestedRequest),
            "encode reentrant dispatch");
        std::vector<Net::RoomCommand> commands;
        NetworkServer server;
        Check(server.SetCommandSink([&](const Net::RoomCommand& command)
        {
            commands.push_back(command);
            if (command.requestId == 17)
            {
                // 콜백에서 같은 접속을 정리하거나 분배를 중첩해도 현재 수신 버퍼는 유지되어야 한다.
                server.DispatchPacket(command.sessionToken, nestedRequest);
                server.OnClientDisconnected(command.sessionToken);
                throw std::runtime_error("fixture dispatch exception");
            }
            if (std::holds_alternative<Net::PlayerTransform>(command.body))
                SetEvent(movementReceived);
        }), "install recording command sink");
        NetworkServerTestAccess::AddClient(server, accepted, token, context, membershipId);
        const std::uint16_t serverPort = FreePort();
        Check(server.StartNetwork(serverPort, 0), "start receive network");
        Check(!server.SetCommandSink({}), "running network keeps its command sink");
        const Net::ServerTimeMs started = GetTickCount64();

        Net::PacketData ready;
        Net::PacketData reload;
        Net::PacketData wrongRoom;
        Net::PacketData wrongMembership;
        Net::PacketData probe;
        Check(Net::PacketCodec::EncodeTcp(Net::ReadyChangeRequest{context.roomId, 1}, 17, ready) &&
            Net::PacketCodec::EncodeTcp(Net::ReloadRequest{context, membershipId}, 18, reload) &&
            Net::PacketCodec::EncodeTcp(Net::MapLoaded{context.roomId + 1}, 19, wrongRoom) &&
            Net::PacketCodec::EncodeTcp(Net::ReloadRequest{context, membershipId - 1}, 20, wrongMembership) &&
            Net::PacketCodec::EncodeTcp(Net::TimeProbeRequest{123}, 21, probe), "encode TCP requests");

        // 첫 요청은 헤더 가운데에서 나누고 나머지는 여러 요청과 함께 보낸다.
        SendBytes(peer, ready.bytes.data(), 3);
        Sleep(30);
        std::vector<std::uint8_t> combined(ready.bytes.begin() + 3, ready.bytes.end());
        combined.insert(combined.end(), reload.bytes.begin(), reload.bytes.end());
        combined.insert(combined.end(), wrongRoom.bytes.begin(), wrongRoom.bytes.end());
        combined.insert(combined.end(), wrongMembership.bytes.begin(), wrongMembership.bytes.end());
        ready.bytes.back() = 2;
        combined.insert(combined.end(), ready.bytes.begin(), ready.bytes.end());
        combined.insert(combined.end(), probe.bytes.begin(), probe.bytes.end());
        SendBytes(peer, combined.data(), combined.size());

        const auto probeBytes = Receive(peer, Net::TcpHeaderBytes + 8 + 8 + 8);
        Net::TcpPacket<Net::TimeProbeResult> probeReply;
        std::size_t consumed = 0;
        Check(Net::PacketCodec::DecodeTcp(probeBytes.data(), probeBytes.size(), probeReply, consumed)
            == Net::DecodeStatus::Complete && probeReply.header.requestId == 21 &&
            probeReply.packetData.clientSendMs == 123 && probeReply.packetData.serverReceiveMs >= started &&
            probeReply.packetData.serverSendMs >= probeReply.packetData.serverReceiveMs,
            "time probe after split and coalesced requests");

        const SOCKET udp = ConnectUdp(NetworkServerTestAccess::UdpPort(server));

        Net::UdpPacket<Net::UdpRegisterRequest> registration;
        registration.header.m_sessionToken = token;
        registration.header.sequence = 1;
        Net::PacketData packet;
        Check(Net::PacketCodec::EncodeUdp(registration, packet), "encode UDP registration");
        std::vector<std::uint8_t> oversized(Net::MaxUdpPacketBytes + 1);
        Check(send(udp, reinterpret_cast<const char*>(oversized.data()), static_cast<int>(oversized.size()), 0)
            == static_cast<int>(oversized.size()), "send oversized datagram");
        Check(send(udp, reinterpret_cast<const char*>(packet.bytes.data()), static_cast<int>(packet.bytes.size()), 0)
            == static_cast<int>(packet.bytes.size()), "send UDP registration");
        const auto registrationBytes = Receive(udp, Net::UdpHeaderBytes + 2 + 4);
        Net::UdpPacket<Net::UdpRegisterResult> registrationReply;
        Check(Net::PacketCodec::DecodeUdp(registrationBytes.data(), registrationBytes.size(), registrationReply) &&
            registrationReply.header.m_sessionToken == token &&
            registrationReply.packetData.registrationSequence == registration.header.sequence, "receive UDP registration");

        Net::UdpPacket<Net::PlayerTransform> movement;
        movement.header.type = Net::UdpType::PlayerTransform;
        movement.header.size = Net::UdpHeaderBytes + 4 + 16 * 4;
        movement.header.m_sessionToken = token;
        movement.header.context = context;
        movement.header.membershipId = membershipId - 1;
        movement.header.sequence = 2;
        movement.packetData.inputSequence = 22;
        Check(Net::PacketCodec::EncodeUdp(movement, packet), "encode stale movement");
        Check(send(udp, reinterpret_cast<const char*>(packet.bytes.data()), static_cast<int>(packet.bytes.size()), 0)
            == static_cast<int>(packet.bytes.size()), "send stale movement");
        movement.header.membershipId = membershipId;
        Check(Net::PacketCodec::EncodeUdp(movement, packet), "encode valid movement");
        Check(send(udp, reinterpret_cast<const char*>(packet.bytes.data()), static_cast<int>(packet.bytes.size()), 0)
            == static_cast<int>(packet.bytes.size()), "send valid movement");
        Check(WaitForSingleObject(movementReceived, 3000) == WAIT_OBJECT_0, "route UDP movement");
        closesocket(udp);

        Check(shutdown(peer, SD_SEND) == 0, "send TCP EOF");
        char byte = 0;
        Check(recv(peer, &byte, 1, 0) == 0, "server disconnects on TCP EOF");
        closesocket(peer);

        // 다른 접속의 잘못된 헤더도 정리하며 다음 접속은 정상 처리한다.
        const SOCKET invalidPeer = Connect(serverPort);
        Receive(invalidPeer, Net::TcpHeaderBytes + 2 + Net::SessionTokenBytes + 2 + 8 + 2);
        probe.bytes[5] ^= 1;
        SendBytes(invalidPeer, probe.bytes.data(), probe.bytes.size());
        Check(recv(invalidPeer, &byte, 1, 0) == 0, "server closes invalid TCP header");
        closesocket(invalidPeer);
        const SOCKET nextPeer = Connect(serverPort);
        Receive(nextPeer, Net::TcpHeaderBytes + 2 + Net::SessionTokenBytes + 2 + 8 + 2);
        server.StopNetwork();
        Check(recv(nextPeer, &byte, 1, 0) == 0, "stop closes remaining TCP peer");
        closesocket(nextPeer);
        CloseHandle(movementReceived);

        Check(commands.size() == 4, "route only validated commands and one disconnect");
        Check(std::holds_alternative<Net::ReadyChangeRequest>(commands[0].body) && commands[0].requestId == 17 &&
            std::get<Net::ReadyChangeRequest>(commands[0].body).ready == 1, "dispatch ready command");
        Check(std::holds_alternative<Net::ReloadRequest>(commands[1].body) && commands[1].requestId == 18,
            "dispatch reload command");
        Check(std::holds_alternative<Net::PlayerTransform>(commands[2].body) &&
            std::get<Net::PlayerTransform>(commands[2].body).inputSequence == 22, "dispatch UDP transform");
        Check(std::holds_alternative<Net::ClientDisconnected>(commands[3].body), "dispatch disconnect");
        for (const auto& command : commands)
            Check(command.sessionToken == token && command.context.roomId == context.roomId &&
                command.context.matchId == context.matchId && command.membershipId == membershipId &&
                command.receivedAtMs >= started, "dispatch server-owned session metadata");
    }

    void TestStopFromCallback()
    {
        SOCKET accepted = INVALID_SOCKET;
        const SOCKET peer = ConnectFixture(accepted);
        Net::SessionToken token{};
        token[0] = 1;
        const std::uint16_t port = FreePort();
        const HANDLE stopReturned = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        Check(stopReturned != nullptr, "create callback stop event");
        std::vector<Net::RoomCommand> commands;
        NetworkServer server;
        Check(server.SetCommandSink([&](const Net::RoomCommand& command)
        {
            commands.push_back(command);
            if (std::holds_alternative<Net::ClientDisconnected>(command.body))
            {
                // 통신 스레드를 합친 뒤 호출되는 퇴장 콜백에서도 종료와 시작을 중첩하지 않는다.
                server.StopNetwork();
                Check(!server.StartNetwork(port, 0), "cleanup callback cannot restart network");
                Check(!server.SetCommandSink({}), "cleanup callback cannot replace itself");
                throw std::runtime_error("fixture disconnect exception");
            }
            server.StopNetwork();
            SetEvent(stopReturned);
        }), "install shutdown command sink");
        NetworkServerTestAccess::AddClient(server, accepted, token, {7, 3}, 11);
        Check(server.StartNetwork(port, 0), "start callback stop network");

        Net::PacketData first;
        Net::PacketData second;
        Check(Net::PacketCodec::EncodeTcp(Net::ReadyChangeRequest{7, 1}, 31, first) &&
            Net::PacketCodec::EncodeTcp(Net::ReadyChangeRequest{7, 0}, 32, second),
            "encode requests around stop");
        first.bytes.insert(first.bytes.end(), second.bytes.begin(), second.bytes.end());
        SendBytes(peer, first.bytes.data(), first.bytes.size());
        Check(WaitForSingleObject(stopReturned, 3000) == WAIT_OBJECT_0, "worker stop returns without joining itself");
        Check(!server.StartNetwork(port, 0), "control thread must finish shutdown before restarting");
        Net::SendRequest pending;
        pending.recipientToken = token;
        pending.packet = second;
        Check(!server.SendPacket(pending), "worker stop request rejects new sends");

        server.StopNetwork();
        server.StopNetwork();
        char byte = 0;
        const int received = recv(peer, &byte, 1, 0);
        Check(received == 0 || (received == SOCKET_ERROR && WSAGetLastError() == WSAECONNRESET),
            "control thread closes stopped client");
        closesocket(peer);
        CloseHandle(stopReturned);
        Check(commands.size() == 2 && commands[0].requestId == 31 &&
            std::holds_alternative<Net::ClientDisconnected>(commands[1].body),
            "shutdown leaves one request and one disconnect");
        Check(server.StartNetwork(port, 0), "restart after callback shutdown");
        server.StopNetwork();
    }

    void TestGuessedUdpToken(std::uint16_t udpPort, const Net::SessionToken& known,
        const Net::SessionToken& target)
    {
        const SOCKET udp = ConnectUdp(udpPort);
        Net::UdpPacket<Net::UdpRegisterRequest> registration;
        registration.header.m_sessionToken = known;

        // 이전의 순번 방식이면 첫 접속 토큰에 1을 더해 다음 접속의 토큰을 맞출 수 있었다.
        for (std::size_t i = known.size(); i > 0; --i)
        {
            ++registration.header.m_sessionToken[i - 1];
            if (registration.header.m_sessionToken[i - 1] != 0)
                break;
        }
        registration.header.sequence = 10;
        Net::PacketData packet;
        Check(Net::PacketCodec::EncodeUdp(registration, packet), "encode guessed UDP token");
        Check(send(udp, reinterpret_cast<const char*>(packet.bytes.data()), static_cast<int>(packet.bytes.size()), 0)
            == static_cast<int>(packet.bytes.size()), "send guessed UDP token");

        registration.header.m_sessionToken = target;
        registration.header.sequence = 11;
        Check(Net::PacketCodec::EncodeUdp(registration, packet), "encode legitimate UDP token");
        Check(send(udp, reinterpret_cast<const char*>(packet.bytes.data()), static_cast<int>(packet.bytes.size()), 0)
            == static_cast<int>(packet.bytes.size()), "send legitimate UDP token");
        const auto bytes = Receive(udp, Net::UdpHeaderBytes + 2 + 4);
        Net::UdpPacket<Net::UdpRegisterResult> response;
        Check(Net::PacketCodec::DecodeUdp(bytes.data(), bytes.size(), response) &&
            response.header.m_sessionToken == target && response.packetData.registrationSequence == 11,
            "only legitimate UDP token registers");
        closesocket(udp);
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
        TestGuessedUdpToken(NetworkServerTestAccess::UdpPort(server), tokens[0], tokens[1]);
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

        int roomStops = 0;
        Framework framework;
        framework.SetRoomStopCallback([&]
        {
            ++roomStops;
            // 종료 콜백이 재진입하거나 예외를 던져도 소켓을 정리해야 한다.
            framework.StopServer();
            framework.SetRoomStopCallback({});
            throw std::runtime_error("room stop fixture");
        });
        Check(framework.StartServer(port, 0), "framework start");
        Check(!framework.StartServer(port, 0), "framework duplicate start");
        const SOCKET frameworkPeer = Connect(port);
        Receive(frameworkPeer, Net::TcpHeaderBytes + 2 + Net::SessionTokenBytes + 2 + 8 + 2);
        framework.StopServer();
        framework.StopServer();
        char frameworkByte = 0;
        Check(recv(frameworkPeer, &frameworkByte, 1, 0) == 0, "framework closes socket after throwing callback");
        closesocket(frameworkPeer);
        Check(roomStops == 1, "stop room once despite reentry");
        Check(framework.StartServer(port, 0), "framework restart after throwing callback");
        framework.StopServer();
        Check(roomStops == 2, "stop callback remains installed after attempted self-replacement");
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
    TestReceiveAndDispatch();
    TestStopFromCallback();
    TestPartialSend();
    TestStartFailure();
    TestUdpFailureAndShutdown();
    WSACleanup();
    std::printf("PASS: server network, %d checks\n", checks);
    return 0;
}
