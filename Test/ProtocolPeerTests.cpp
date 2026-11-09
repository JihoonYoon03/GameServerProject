#include "ProtocolPeer.h"
#include "../Tests/NetworkFixtures.h"

#include <iostream>
#include <thread>

namespace
{
    Net::PacketData MakeRoomListReply(std::uint32_t requestId)
    {
        Net::TcpPacket<Net::RoomListResult> reply;
        reply.header.type = Net::TcpType::RoomListResult;
        reply.header.size = Net::TcpHeaderBytes + 4 + 2;
        reply.header.requestId = requestId;
        Net::PacketData encoded;
        Net::PacketCodec::EncodeTcp(reply, encoded);
        return encoded;
    }

    bool FeedSplitTcp(const Net::PacketData& frame)
    {
        ProtocolPeer peer;
        Net::TcpPacket<Net::RoomListResult> reply;
        const std::span<const std::uint8_t> bytes(frame.bytes);

        // 헤더 일부와 본문을 나눠 넣어도 완성 전에는 프레임을 꺼내지 않습니다.
        peer.FeedTcpBytes(bytes.first(3));
        if (peer.TryReadFrame(reply) != Net::DecodeStatus::NeedMore) return false;

        peer.FeedTcpBytes(bytes.subspan(3, Net::TcpHeaderBytes - 3));
        if (peer.TryReadFrame(reply) != Net::DecodeStatus::NeedMore) return false;

        peer.FeedTcpBytes(bytes.subspan(Net::TcpHeaderBytes));
        return peer.TryReadFrame(reply) == Net::DecodeStatus::Complete && reply.header.requestId == 1 &&
            peer.TryReadFrame(reply) == Net::DecodeStatus::NeedMore;
    }

    bool FeedCoalescedTcp(const Net::PacketData& first, const Net::PacketData& second)
    {
        ProtocolPeer peer;
        auto bytes = first.bytes;
        bytes.insert(bytes.end(), second.bytes.begin(), second.bytes.end());
        peer.FeedTcpBytes(bytes);

        // 한 번에 받은 두 프레임을 요청 순서대로 한 개씩 꺼냅니다.
        Net::TcpPacket<Net::RoomListResult> reply;
        return peer.TryReadFrame(reply) == Net::DecodeStatus::Complete && reply.header.requestId == 1 &&
            peer.TryReadFrame(reply) == Net::DecodeStatus::Complete && reply.header.requestId == 2 &&
            peer.TryReadFrame(reply) == Net::DecodeStatus::NeedMore;
    }

    bool FeedInvalidFrame(const Net::PacketData& frame)
    {
        ProtocolPeer peer;
        Net::TcpPacket<Net::RoomListResult> reply;
        auto bytes = frame.bytes;

        // 전체 길이 필드를 0으로 바꾸면 헤더 검사에서 거부됩니다.
        bytes[3] = 0;
        peer.FeedTcpBytes(bytes);
        if (peer.TryReadFrame(reply) != Net::DecodeStatus::Invalid) return false;
        peer.Close();

        // 빈 방 목록의 마지막 두 바이트는 방 개수입니다. 하위 바이트를 상한보다 크게 설정합니다.
        bytes = frame.bytes;
        bytes.back() = static_cast<std::uint8_t>(Net::MaxRoomsPerPage + 1);
        peer.FeedTcpBytes(bytes);
        return peer.TryReadFrame(reply) == Net::DecodeStatus::Invalid;
    }

    struct Winsock
    {
        WSADATA data{};
        const bool started = WSAStartup(MAKEWORD(2, 2), &data) == 0;
        ~Winsock() { if (started) WSACleanup(); }
    };

    struct Socket
    {
        explicit Socket(SOCKET handle) : handle(handle) {}
        ~Socket() { if (handle != INVALID_SOCKET) closesocket(handle); }
        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;

        const SOCKET handle;
    };

    bool BindLoopback(SOCKET socketHandle, sockaddr_in& address)
    {
        // 운영체제가 빈 포트를 고르게 해 다른 테스트와의 포트 충돌을 피합니다.
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        int length = sizeof(address);
        return socketHandle != INVALID_SOCKET &&
            bind(socketHandle, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0 &&
            getsockname(socketHandle, reinterpret_cast<sockaddr*>(&address), &length) == 0;
    }

    bool SetTimeout(SOCKET socketHandle, int option)
    {
        const DWORD timeoutMs = 3000;
        return setsockopt(socketHandle, SOL_SOCKET, option,
            reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs)) == 0;
    }

    bool SendTcp(SOCKET socketHandle, const Net::PacketData& packet)
    {
        if (packet.bytes.empty()) return false;

        std::size_t offset = 0;
        while (offset < packet.bytes.size())
        {
            const int sent = send(socketHandle, reinterpret_cast<const char*>(packet.bytes.data() + offset),
                static_cast<int>(packet.bytes.size() - offset), 0);
            if (sent <= 0) return false;
            offset += sent;
        }
        return true;
    }

    bool ReplyToRegistration(SOCKET udpSocket, const Net::SessionToken& token)
    {
        std::array<std::uint8_t, Net::MaxUdpPacketBytes> bytes{};
        sockaddr_in sender{};
        int senderLength = sizeof(sender);
        const int received = recvfrom(udpSocket, reinterpret_cast<char*>(bytes.data()),
            static_cast<int>(bytes.size()), 0, reinterpret_cast<sockaddr*>(&sender), &senderLength);
        Net::UdpPacket<Net::UdpRegisterRequest> request;
        if (received <= 0 || !Net::PacketCodec::DecodeUdp(bytes.data(), static_cast<std::size_t>(received), request) ||
            request.header.m_sessionToken != token) return false;

        // 요청 토큰과 순번을 그대로 돌려주어 실제 등록 응답의 확인 경로를 통과시킵니다.
        Net::UdpPacket<Net::UdpRegisterResult> reply;
        reply.header.type = Net::UdpType::RegisterResult;
        reply.header.size = Net::UdpHeaderBytes + 2 + 4;
        reply.header.m_sessionToken = token;
        reply.header.sequence = request.header.sequence;
        reply.packetData.registrationSequence = request.header.sequence;
        Net::PacketData encoded;
        if (!Net::PacketCodec::EncodeUdp(reply, encoded)) return false;

        const auto size = static_cast<int>(encoded.bytes.size());
        return sendto(udpSocket, reinterpret_cast<const char*>(encoded.bytes.data()), size, 0,
            reinterpret_cast<const sockaddr*>(&sender), senderLength) == size;
    }

    bool ServePeer(SOCKET listener, SOCKET udpSocket, std::uint16_t udpPort)
    {
        fd_set ready;
        FD_ZERO(&ready);
        FD_SET(listener, &ready);
        timeval timeout{3, 0};
        if (select(0, &ready, nullptr, nullptr, &timeout) != 1) return false;

        const Socket client(accept(listener, nullptr, nullptr));
        if (client.handle == INVALID_SOCKET || !SetTimeout(client.handle, SO_RCVTIMEO) ||
            !SetTimeout(client.handle, SO_SNDTIMEO)) return false;

        // 게임 서버 대신 접속, UDP 등록, 방 목록 요청에만 응답합니다.
        const auto token = NetworkFixtures::Token();
        Net::TcpPacket<Net::ConnectResult> greeting;
        greeting.header.size = Net::TcpHeaderBytes + 2 + Net::SessionTokenBytes + 2 + 8 + 2;
        greeting.packetData.sessionToken = token;
        greeting.packetData.udpPort = udpPort;
        Net::PacketData encoded;
        if (!Net::PacketCodec::EncodeTcp(greeting, encoded) || !SendTcp(client.handle, encoded) ||
            !ReplyToRegistration(udpSocket, token)) return false;

        // 이 대역은 길이가 고정된 방 목록 요청 한 개만 읽습니다.
        std::array<std::uint8_t, Net::TcpHeaderBytes + 4> bytes{};
        std::size_t offset = 0;
        while (offset < bytes.size())
        {
            const int received = recv(client.handle, reinterpret_cast<char*>(bytes.data() + offset),
                static_cast<int>(bytes.size() - offset), 0);
            if (received <= 0) return false;
            offset += received;
        }

        Net::TcpPacket<Net::RoomListRequest> request;
        std::size_t consumed = 0;
        if (Net::PacketCodec::DecodeTcp(bytes.data(), bytes.size(), request, consumed) != Net::DecodeStatus::Complete)
            return false;

        auto reply = MakeRoomListReply(request.header.requestId);
        if (!SendTcp(client.handle, reply)) return false;

        // 다음 응답의 끝을 잘라 보낸 뒤 송신을 종료해 미완성 프레임의 종료 처리를 확인합니다.
        reply.bytes.pop_back();
        return SendTcp(client.handle, reply) && shutdown(client.handle, SD_SEND) == 0;
    }

    bool ExchangeWithLoopback()
    {
        const Winsock winsock;
        if (!winsock.started) return false;

        const Socket listener(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        const Socket udpSocket(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
        sockaddr_in tcpAddress{};
        sockaddr_in udpAddress{};
        if (!BindLoopback(listener.handle, tcpAddress) || listen(listener.handle, 1) != 0 ||
            !BindLoopback(udpSocket.handle, udpAddress) || !SetTimeout(udpSocket.handle, SO_RCVTIMEO) ||
            !SetTimeout(udpSocket.handle, SO_SNDTIMEO)) return false;

        Net::TcpPacket<Net::RoomListRequest> request;
        request.header.type = Net::TcpType::RoomListRequest;
        request.header.size = Net::TcpHeaderBytes + 4;
        request.header.requestId = 7;
        Net::PacketData encoded;
        if (!Net::PacketCodec::EncodeTcp(request, encoded)) return false;

        bool serverSucceeded = false;
        std::jthread server([&]
        {
            serverSucceeded = ServePeer(listener.handle, udpSocket.handle, ntohs(udpAddress.sin_port));
        });
        ProtocolPeer peer;
        Net::TcpPacket<Net::RoomListResult> reply;
        const bool peerSucceeded = peer.Connect(tcpAddress) && peer.RegisterUdp() && peer.SendCommand(encoded) &&
            peer.ReceiveFrame(reply) && reply.header.requestId == request.header.requestId && reply.packetData.rooms.empty() &&
            !peer.ReceiveFrame(reply);

        // 실패 시에도 먼저 접속을 닫아 대역의 수신 대기를 끝냅니다.
        peer.Close();
        server.join();
        return peerSucceeded && serverSucceeded;
    }
}

bool RunProtocolPeerTests()
{
    const auto first = MakeRoomListReply(1);
    const auto second = MakeRoomListReply(2);
    if (first.bytes.empty() || second.bytes.empty())
    {
        std::cerr << "FAIL: encode protocol peer fixtures\n";
        return false;
    }

    const bool split = FeedSplitTcp(first);
    const bool coalesced = FeedCoalescedTcp(first, second);
    const bool invalid = FeedInvalidFrame(first);
    const bool exchange = ExchangeWithLoopback();
    std::cout << (split ? "PASS" : "FAIL") << ": split TCP frames\n"
        << (coalesced ? "PASS" : "FAIL") << ": coalesced TCP frames\n"
        << (invalid ? "PASS" : "FAIL") << ": invalid TCP frames\n"
        << (exchange ? "PASS" : "FAIL") << ": protocol peer TCP/UDP exchange\n";
    return split && coalesced && invalid && exchange;
}
