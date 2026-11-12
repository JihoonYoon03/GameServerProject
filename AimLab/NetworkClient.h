#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <Windows.h>
#include "../Shared/NetworkMessages.h"
#include <atomic>
#include <queue>
#include <string>

class NetworkClient
{
public:
    NetworkClient();
    ~NetworkClient();
    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;

    bool ConnectServer(const std::string& address, std::uint16_t tcpPort);
    // 최초 접속 응답을 받은 통신 스레드가 한 번만 등록한다.
    void RegisterUDP();
    void CompleteConnection(const Net::UdpRegisterResult& result);
    bool StartNetwork();
    void NetworkLoop();
    bool SendPacket(const Net::PacketData& packet);
    // 메인 스레드에서 검증된 수신 큐를 이번 프레임의 묶음으로 옮긴다.
    void UpdateNetwork();
    // 메인이 통신 스레드의 종료를 기다린 뒤 소켓과 큐를 해제한다.
    void DisconnectServer();

    bool IsConnected() const { return m_UDPRegistered && !m_bStopRequested; }
    bool IsDisconnected() const { return m_bStopRequested; }
    // 메인에서만 등록 완료 후 토큰과 이번 프레임의 수신 목록을 읽는다.
    Net::SessionToken GetSessionToken() const;
    const std::vector<Net::PacketData>& GetReceivedPackets() const { return m_ReceivedPackets; }
    std::uint32_t NextRequestId();

private:
    static DWORD WINAPI NetworkProc(LPVOID parameter);
    bool ReceiveTcp();
    void ReceiveUdp();
    bool ProcessTcpPacket(const Net::PacketData& packet);
    bool FlushSendQueue();
    void QueueReceived(const Net::PacketData& packet);
    void SetReceiveContext(Net::MatchContext context, Net::MembershipId membershipId);

    SOCKET m_TCPSocket = INVALID_SOCKET;
    SOCKET m_UDPSocket = INVALID_SOCKET;
    sockaddr_in m_ServerTcpAddr{};
    sockaddr_in m_ServerUdpAddr{};
    Net::SessionToken m_SessionToken{};
    HANDLE m_hNetworkThread = nullptr;
    const DWORD m_ControlThreadId;
    std::atomic<DWORD> m_NetworkThreadId{0};
    std::atomic<bool> m_bStopRequested{true};
    std::atomic<bool> m_UDPRegistered{false};
    bool m_WinsockStarted = false;
    bool m_TcpConnecting = false;
    bool m_HasConnectResult = false;
    bool m_RegistrationSent = false;
    Net::Sequence m_RegistrationSequence = 0;

    CRITICAL_SECTION m_CSSendQueue;
    CRITICAL_SECTION m_CSReceiveQueue;
    std::queue<Net::PacketData> m_qSendQueue;
    std::queue<Net::PacketData> m_qReceiveQueue;
    std::vector<Net::PacketData> m_ReceivedPackets;
    // 방 자동 생성 여부와 다음 요청 번호는 메인 스레드만 바꾼다.
    bool m_RoomRequested = false;
    std::uint32_t m_NextRequestId = 1;
    // 입장 응답을 대조할 요청 번호는 메인과 통신 스레드가 공유한다.
    std::atomic<std::uint32_t> m_PendingRoomRequestId{0};
    std::vector<std::uint8_t> m_TcpReceiveBuffer;
    Net::PacketData m_CurrentSend;
    bool m_HasCurrentSend = false;
    std::size_t m_SendOffset = 0;

    // 이 상태는 통신 스레드가 TCP 방 응답과 UDP 수신을 비교할 때만 사용한다.
    Net::MatchContext m_ReceiveContext{};
    Net::MembershipId m_MembershipId = 0;
    Net::Sequence m_LastPlayerSequence = 0;
    Net::Sequence m_LastTargetSequence = 0;
    bool m_HasPlayerSequence = false;
    bool m_HasTargetSequence = false;
};
