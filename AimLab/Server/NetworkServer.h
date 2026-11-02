#pragma once
#include "../Shared/NetworkMessages.h"

struct sockaddr_in;

namespace Server
{
    class NetworkServer
    {
    public:
        bool StartNetwork(std::uint16_t tcpPort, std::uint16_t udpPort);
        void NetworkLoop();
        void AcceptPlayer();
        // 토큰과 실제 수신 주소를 연결한다. 토큰만으로 발신자를 인증하지 않는다.
        void RegisterPlayerUDP(const Net::UdpHeader& header,
            const sockaddr_in& sourceAddress);
        void OnClientDisconnected(const Net::SessionToken& sessionToken);
        // 확인된 발신자와 패킷을 방 명령 큐로 전달하며 게임 상태는 직접 바꾸지 않는다.
        void DispatchPacket(const Net::SessionToken& verifiedSender,
            const Net::PacketData& packet);
        // 방 스레드의 송신 요청을 큐에 넣고 실제 소켓 송신은 통신 스레드가 담당한다.
        bool SendPacket(const Net::SendRequest& request);
        void ReplyTimeProbe(const Net::SessionToken& recipient,
            const Net::TimeProbeRequest& request, Net::ServerTimeMs receivedAtMs);
        void StopNetwork();
    };
}
