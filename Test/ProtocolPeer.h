#pragma once

#include <winsock2.h>
#include <span>
#include <utility>
#include "../Shared/PacketCodec.h"

// 화면 없이 서버 패킷을 주고받는 테스트 전용 클라이언트입니다.
class ProtocolPeer
{
public:
    ProtocolPeer() = default;
    ~ProtocolPeer();
    ProtocolPeer(const ProtocolPeer&) = delete;
    ProtocolPeer& operator=(const ProtocolPeer&) = delete;

    bool Connect(const sockaddr_in& tcpAddress);
    bool RegisterUdp();
    bool SendCommand(const Net::PacketData& packet);
    void Close();

    void FeedTcpBytes(std::span<const std::uint8_t> bytes);

    template<class T>
    Net::DecodeStatus TryReadFrame(Net::TcpPacket<T>& packet)
    {
        std::size_t consumed = 0;
        const auto status = Net::PacketCodec::DecodeTcp(
            receiveBuffer_.data(), receiveBuffer_.size(), packet, consumed);

        // 완성된 프레임만 꺼냅니다. 다음 프레임과 미완성 바이트는 버퍼에 남깁니다.
        if (status == Net::DecodeStatus::Complete)
            receiveBuffer_.erase(receiveBuffer_.begin(), receiveBuffer_.begin() + consumed);

        return status;
    }

    template<class T>
    bool ReceiveFrame(Net::TcpPacket<T>& packet)
    {
        while (tcpSocket_ != INVALID_SOCKET)
        {
            const auto status = TryReadFrame(packet);
            if (status == Net::DecodeStatus::Complete) return true;
            if (status == Net::DecodeStatus::Invalid || !ReceiveTcpBytes()) break;
        }

        // 오류 또는 연결 종료 뒤에는 이전 연결의 미완성 바이트를 사용하지 않습니다.
        Close();
        return false;
    }

    template<class T>
    bool ReceiveFrame(Net::UdpPacket<T>& packet)
    {
        if (udpSocket_ == INVALID_SOCKET) return false;

        std::array<std::uint8_t, Net::MaxUdpPacketBytes> bytes{};
        const int received = recv(udpSocket_, reinterpret_cast<char*>(bytes.data()),
            static_cast<int>(bytes.size()), 0);
        Net::UdpPacket<T> decoded;

        // UDP는 데이터그램 하나를 통째로 해석하고 수신자 토큰을 확인합니다.
        if (received <= 0 || !Net::PacketCodec::DecodeUdp(bytes.data(), static_cast<std::size_t>(received), decoded) ||
            decoded.header.m_sessionToken != token_) return false;

        packet = std::move(decoded);
        return true;
    }

private:
    bool ReceiveTcpBytes();

    SOCKET tcpSocket_ = INVALID_SOCKET;
    SOCKET udpSocket_ = INVALID_SOCKET;
    sockaddr_in udpAddress_{};
    Net::SessionToken token_{};
    std::vector<std::uint8_t> receiveBuffer_;
    bool winsockStarted_ = false;
    bool udpRegistered_ = false;
};
