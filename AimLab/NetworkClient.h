#pragma once
#include "Shared/NetworkMessages.h"
#include <string>

// 클라이언트 통신 함수 선언. 실제 소켓·통신 스레드 처리는 이후 구현한다.
class NetworkClient
{
public:
    bool ConnectServer(const std::string& address, std::uint16_t tcpPort);
    void RegisterUDP();
    void CompleteConnection(const Net::UdpRegisterResult& result);
    bool StartNetwork();
    void NetworkLoop();
    // 메인 스레드에서 송신 큐에 추가하고, 실제 송신은 통신 스레드에서 처리한다.
    bool SendPacket(const Net::PacketData& packet);
    // 메인 스레드에서만 수신 큐를 꺼내 장면에 반영한다.
    void UpdateNetwork();
    void SendTimeProbe();
    void ApplyTimeProbe(const Net::TimeProbeResult& result);
    // 종료 요청 후 통신 스레드가 끝났는지 확인하고 연결 자원을 정리한다.
    void DisconnectServer();
};
