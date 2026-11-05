#pragma once
#include "Shared/NetworkMessages.h"
#include <string>

// 클라이언트 통신 함수 선언. 실제 소켓·통신 스레드 처리는 이후 구현한다.
class NetworkClient
{
public:
    //여기서 TCP 접속
    bool ConnectServer(const std::string& address, std::uint16_t tcpPort);
    //여기서 토큰으로 UDP 연결 시도
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

private:
    SOCKET      m_TCPSocket;
    SOCKET      m_UDPSocket;

    sockaddr_in         m_ServerAddr;

    Net::SessionToken   m_SessionToken;
    //UDP 등록 여부
    std::atomic<bool>   m_UDPRegistered{ false };

    //CreateThread에 전달되는 네트워크 쓰레드 함수. static없으면 인자값으로 사용 못 함 주의
    static DWORD WINAPI NetworkProc(LPVOID param);
    HANDLE              m_hNetworkThread;
    //쓰레드 종료 여부
    std::atomic<bool>   m_bStopRequested{ false };

    //서버에게 줄 데이터와 받은 데이터를 큐에 저장하고
    //관련 처리 함수에서 일괄적으로 처리함.
    //SendRequest엔 현재 클라의 토큰과 패킷 데이터가 있음
    std::queue<Net::SendRequest>    m_qSendQueue;
    std::queue<Net::PacketData>    m_qReceiveQueue;
};
