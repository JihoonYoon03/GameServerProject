#include "ProtocolPeer.h"

namespace
{
    bool SetTimeouts(SOCKET socketHandle)
    {
        // 테스트가 실패해도 송수신에서 무한정 대기하지 않도록 제한합니다.
        const DWORD timeoutMs = 3000;
        const auto* value = reinterpret_cast<const char*>(&timeoutMs);
        return setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, value, sizeof(timeoutMs)) == 0 &&
            setsockopt(socketHandle, SOL_SOCKET, SO_SNDTIMEO, value, sizeof(timeoutMs)) == 0;
    }

    void CloseSocket(SOCKET& socketHandle)
    {
        if (socketHandle == INVALID_SOCKET) return;
        closesocket(socketHandle);
        socketHandle = INVALID_SOCKET;
    }
}

ProtocolPeer::~ProtocolPeer()
{
    Close();
}

bool ProtocolPeer::Connect(const sockaddr_in& tcpAddress)
{
    Close();

    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
    winsockStarted_ = true;

    tcpSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcpSocket_ == INVALID_SOCKET || !SetTimeouts(tcpSocket_) ||
        connect(tcpSocket_, reinterpret_cast<const sockaddr*>(&tcpAddress), sizeof(tcpAddress)) != 0)
    {
        Close();
        return false;
    }

    // 접속 응답에서 이후 UDP 등록에 사용할 토큰과 서버 포트를 얻습니다.
    Net::TcpPacket<Net::ConnectResult> reply;
    if (!ReceiveFrame(reply) || reply.packetData.result != Net::ErrorCode::Ok ||
        !Net::IsValidSessionToken(reply.packetData.sessionToken) || reply.packetData.udpPort == 0)
    {
        Close();
        return false;
    }

    token_ = reply.packetData.sessionToken;
    udpAddress_ = tcpAddress;
    udpAddress_.sin_port = htons(reply.packetData.udpPort);
    return true;
}

bool ProtocolPeer::RegisterUdp()
{
    if (tcpSocket_ == INVALID_SOCKET) return false;
    if (udpRegistered_) return true;

    if (udpSocket_ == INVALID_SOCKET)
    {
        udpSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udpSocket_ == INVALID_SOCKET || !SetTimeouts(udpSocket_) ||
            connect(udpSocket_, reinterpret_cast<const sockaddr*>(&udpAddress_), sizeof(udpAddress_)) != 0)
        {
            CloseSocket(udpSocket_);
            return false;
        }
    }

    // 등록 응답이 유실되면 같은 소켓과 요청 순번으로 다시 시도할 수 있습니다.
    constexpr Net::Sequence RegistrationSequence = 1;
    Net::UdpPacket<Net::UdpRegisterRequest> request;
    request.header.m_sessionToken = token_;
    request.header.sequence = RegistrationSequence;

    Net::PacketData encoded;
    if (!Net::PacketCodec::EncodeUdp(request, encoded)) return false;

    const auto size = static_cast<int>(encoded.bytes.size());
    if (send(udpSocket_, reinterpret_cast<const char*>(encoded.bytes.data()), size, 0) != size) return false;

    Net::UdpPacket<Net::UdpRegisterResult> reply;
    udpRegistered_ = ReceiveFrame(reply) && reply.header.sequence == RegistrationSequence &&
        reply.packetData.registrationSequence == RegistrationSequence && reply.packetData.result == Net::ErrorCode::Ok;
    return udpRegistered_;
}

bool ProtocolPeer::SendCommand(const Net::PacketData& packet)
{
    if (packet.transport == Net::PacketTransport::Udp)
    {
        if (!udpRegistered_ || packet.bytes.empty() || packet.bytes.size() > Net::MaxUdpPacketBytes) return false;

        const auto size = static_cast<int>(packet.bytes.size());
        return send(udpSocket_, reinterpret_cast<const char*>(packet.bytes.data()), size, 0) == size;
    }

    if (packet.transport != Net::PacketTransport::Tcp || tcpSocket_ == INVALID_SOCKET ||
        packet.bytes.empty() || packet.bytes.size() > Net::MaxTcpPacketBytes) return false;

    // TCP가 일부만 보내면 아직 보내지 않은 바이트부터 이어서 보냅니다.
    std::size_t offset = 0;
    while (offset < packet.bytes.size())
    {
        const int sent = send(tcpSocket_, reinterpret_cast<const char*>(packet.bytes.data() + offset),
            static_cast<int>(packet.bytes.size() - offset), 0);
        if (sent <= 0)
        {
            Close();
            return false;
        }
        offset += sent;
    }
    return true;
}

void ProtocolPeer::FeedTcpBytes(std::span<const std::uint8_t> bytes)
{
    receiveBuffer_.insert(receiveBuffer_.end(), bytes.begin(), bytes.end());
}

bool ProtocolPeer::ReceiveTcpBytes()
{
    std::array<std::uint8_t, 4096> bytes{};
    const int received = recv(tcpSocket_, reinterpret_cast<char*>(bytes.data()), static_cast<int>(bytes.size()), 0);
    if (received <= 0) return false;

    FeedTcpBytes({bytes.data(), static_cast<std::size_t>(received)});
    return true;
}

void ProtocolPeer::Close()
{
    CloseSocket(udpSocket_);
    CloseSocket(tcpSocket_);
    udpAddress_ = {};
    token_ = {};
    receiveBuffer_.clear();
    udpRegistered_ = false;

    if (winsockStarted_)
    {
        WSACleanup();
        winsockStarted_ = false;
    }
}
