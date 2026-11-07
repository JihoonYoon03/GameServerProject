#include "pch.h"
#include "NetworkServer.h"
#include "../Shared/PacketCodec.h"

void NetworkServer::RegisterPlayerUDP(const Net::UdpHeader& header, const sockaddr_in& sourceAddress)
{
	//int retval = 0;
	////헤더 정보 확인은 NetworkLoop에서 처리할 예정
	//
	////AcceptPlayer에서 클라이언트 TCP 접속을 받고, 토큰 저장한 뒤 클라에게 알림
	////UDP 연결 시도에서 해당 토큰을 클라에서 보내옴
	//for (Client& client : m_vClients)
	//{
	//	//세션 토큰 불일치 클라는 넘기기
	//	if (client.m_sessionToken != header.m_sessionToken)
	//		continue;

	//	Net::UdpRegisterResult regResult;
	//	//시퀀스는 패킷 순번을 의미함. 해당 값으로 어떤 요청 답변이 들어왔는지 확인 가능
	//	regResult.registrationSequence = header.sequence;

	//	sockaddr_in tcpPeer{};
	//	int addrlen = sizeof(sockaddr_in);
	//	retval = getpeername(client.m_TCPSocket, (sockaddr*)&tcpPeer, &addrlen);

	//	// 소켓 에러
	//	if (retval == SOCKET_ERROR)
	//	{
	//		err_display("getpeername()");
	//		regResult.result = Net::ErrorCode::UdpNotRegistered;
	//	}
	//	// UDP 연결 시도한 클라의 주소체계가 다르거나
	//	// 토큰이 같은 클라이언트의 TCP IP주소와 다른 경우
	//	else if (sourceAddress.sin_family != AF_INET ||
	//			tcpPeer.sin_addr.s_addr != sourceAddress.sin_addr.s_addr)
	//	{
	//		regResult.result = Net::ErrorCode::UdpNotRegistered;
	//	}
	//	else
	//	{

	//		client.m_UDPAddress = sourceAddress;
	//		client.m_bUDPRegistered = true;
	//	}

	//	retval = sendto(
	//		m_SockUDP,
	//		(char*)&regResult,
	//		sizeof(regResult),
	//		0,
	//		(sockaddr*)&sourceAddress,
	//		sizeof(sourceAddress)
	//	);
	//	if (retval == SOCKET_ERROR)	err_display("sendto()");

	//	return;
	//}
}


NetworkServer::NetworkServer()
{
    InitializeCriticalSection(&m_CSSendQueue);
}

NetworkServer::~NetworkServer()
{
    StopNetwork();
    DeleteCriticalSection(&m_CSSendQueue);
}

bool NetworkServer::SendPacket(const Net::SendRequest& request)
{
    if (request.packet.bytes.empty())
        return false;

    EnterCriticalSection(&m_CSSendQueue);
    if (m_bStopRequested)
    {
        LeaveCriticalSection(&m_CSSendQueue);
        return false;
    }
    m_qSendQueue.push(request);
    LeaveCriticalSection(&m_CSSendQueue);
    return true;
}

void NetworkServer::FlushSendQueue()
{
    // 잠금 안에서는 요청만 옮기고 소켓 송신은 통신 스레드에서 처리한다.
    std::queue<Net::SendRequest> requests;
    EnterCriticalSection(&m_CSSendQueue);
    requests.swap(m_qSendQueue);
    LeaveCriticalSection(&m_CSSendQueue);

    while (!requests.empty())
    {
        Net::SendRequest& request = requests.front();
        for (Client& client : m_vClients)
        {
            if (client.GetSessionToken() != request.recipientToken ||
                client.GetTCPSocket() == INVALID_SOCKET)
                continue;

            switch (request.packet.transport)
            {
            case Net::PacketTransport::Tcp:
                client.QueueSend(request.packet);
                break;
            case Net::PacketTransport::Udp:
                // UDP 등록을 마친 수신자에게만 완성된 데이터그램을 보낸다.
                if (client.IsUDPRegistered())
                {
                    const sockaddr_in& address = client.GetUDPAddress();
                    int sent = sendto(m_SockUDP,
                        reinterpret_cast<const char*>(request.packet.bytes.data()),
                        static_cast<int>(request.packet.bytes.size()), 0,
                        reinterpret_cast<const sockaddr*>(&address), sizeof(address));
                    if (sent == SOCKET_ERROR)
                        err_display("sendto()");
                }
                break;
            }
            break;
        }
        requests.pop();
    }
}

void NetworkServer::SendTcpPending(Client& client)
{
    // 윤지훈 수정 필요: Client::FlushSend의 연결 오류에서 송신 위치에 -1을 더하지 않도록 처리한다.
    // 윤지훈 수정 필요: Client 이동 생성과 대입에서 UDP 등록 순번과 등록 순번 보유 상태도 옮긴다.
    // Client가 보관한 부분 송신 위치부터 이어서 전송한다.
    if (client.GetTCPSocket() != INVALID_SOCKET && client.HasPendingSend())
        client.FlushSend();
}


bool NetworkServer::StartNetwork(std::uint16_t tcpPort, std::uint16_t udpPort)
{
    if (m_hNetworkThread != nullptr)
        return false;

    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        return false;
    m_bWinsockStarted = true;

    m_SockListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    m_SockUDP = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_SockListen == INVALID_SOCKET || m_SockUDP == INVALID_SOCKET)
    {
        StopNetwork();
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(tcpPort);
    if (bind(m_SockListen, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
        listen(m_SockListen, SOMAXCONN) == SOCKET_ERROR)
    {
        err_display("TCP bind/listen()");
        StopNetwork();
        return false;
    }
    address.sin_port = htons(udpPort);
    if (bind(m_SockUDP, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
    {
        err_display("UDP bind()");
        StopNetwork();
        return false;
    }
    int addressSize = sizeof(address);
    if (getsockname(m_SockUDP, reinterpret_cast<sockaddr*>(&address), &addressSize) == SOCKET_ERROR)
    {
        StopNetwork();
        return false;
    }
    m_UDPPort = ntohs(address.sin_port);

    u_long nonblocking = 1;
    if (ioctlsocket(m_SockListen, FIONBIO, &nonblocking) == SOCKET_ERROR ||
        ioctlsocket(m_SockUDP, FIONBIO, &nonblocking) == SOCKET_ERROR)
    {
        StopNetwork();
        return false;
    }

    FILETIME startedAt{};
    GetSystemTimeAsFileTime(&startedAt);
    m_ServerStartedAt = (static_cast<std::uint64_t>(startedAt.dwHighDateTime) << 32) |
        startedAt.dwLowDateTime;
    m_bStopRequested = false;
    m_hNetworkThread = CreateThread(nullptr, 0, NetworkProc, this, 0, nullptr);
    if (m_hNetworkThread == nullptr)
    {
        StopNetwork();
        return false;
    }
    return true;
}

DWORD WINAPI NetworkServer::NetworkProc(LPVOID parameter)
{
    NetworkServer* server = static_cast<NetworkServer*>(parameter);
    server->NetworkLoop();
    return 0;
}

void NetworkServer::NetworkLoop()
{
    while (!m_bStopRequested)
    {
        FlushSendQueue();
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(m_SockListen, &readSet);

        // 접속 대기를 짧게 나누어 송신 큐와 종료 요청도 계속 확인한다.
        timeval timeout{};
        timeout.tv_usec = 10000;
        int ready = select(0, &readSet, nullptr, nullptr, &timeout);
        if (ready == SOCKET_ERROR)
        {
            err_display("select()");
            m_bStopRequested = true;
            break;
        }
        if (FD_ISSET(m_SockListen, &readSet))
            AcceptPlayer();

        for (Client& client : m_vClients)
            SendTcpPending(client);
        for (std::size_t i = 0; i < m_vClients.size();)
        {
            if (m_vClients[i].GetTCPSocket() == INVALID_SOCKET)
                m_vClients.erase(m_vClients.begin() + i);
            else
                ++i;
        }
        // 수신 프레이밍과 방 명령 전달은 계획서의 11/9 담당 구현에서 연결한다.
    }
}

void NetworkServer::StopNetwork()
{
    // 오늘 실행한 통신 스레드를 먼저 종료한 뒤 소유 자원을 정리한다.
    // 방 퇴장 통지와 방 스레드 종료 연결은 후속 일정에서 구현한다.
    EnterCriticalSection(&m_CSSendQueue);
    m_bStopRequested = true;
    LeaveCriticalSection(&m_CSSendQueue);
    if (m_hNetworkThread != nullptr)
    {
        WaitForSingleObject(m_hNetworkThread, INFINITE);
        CloseHandle(m_hNetworkThread);
        m_hNetworkThread = nullptr;
    }
    m_vClients.clear();
    if (m_SockListen != INVALID_SOCKET)
    {
        closesocket(m_SockListen);
        m_SockListen = INVALID_SOCKET;
    }
    if (m_SockUDP != INVALID_SOCKET)
    {
        closesocket(m_SockUDP);
        m_SockUDP = INVALID_SOCKET;
    }
    EnterCriticalSection(&m_CSSendQueue);
    while (!m_qSendQueue.empty())
        m_qSendQueue.pop();
    LeaveCriticalSection(&m_CSSendQueue);
    if (m_bWinsockStarted)
    {
        WSACleanup();
        m_bWinsockStarted = false;
    }
}


void NetworkServer::AcceptPlayer()
{
    sockaddr_in address{};
    int addressSize = sizeof(address);
    SOCKET socketHandle = accept(m_SockListen,
        reinterpret_cast<sockaddr*>(&address), &addressSize);
    if (socketHandle == INVALID_SOCKET)
    {
        if (WSAGetLastError() != WSAEWOULDBLOCK)
            err_display("accept()");
        return;
    }
    u_long nonblocking = 1;
    if (ioctlsocket(socketHandle, FIONBIO, &nonblocking) == SOCKET_ERROR || m_NextSessionId == 0)
    {
        closesocket(socketHandle);
        return;
    }

    // 실행 시각과 접속마다 증가하는 번호로 16바이트 식별자를 만든다.
    Net::SessionToken token{};
    for (int i = 0; i < 8; ++i)
    {
        token[i] = static_cast<std::uint8_t>(m_ServerStartedAt >> (56 - i * 8));
        token[i + 8] = static_cast<std::uint8_t>(m_NextSessionId >> (56 - i * 8));
    }
    ++m_NextSessionId;

    Client client;
    client.SetConnection(socketHandle, token);
    Net::TcpPacket<Net::ConnectResult> response;
    response.header.type = Net::TcpType::ConnectResult;
    response.header.size = Net::TcpHeaderBytes + 2 + Net::SessionTokenBytes + 2 + 8 + 2;
    response.packetData.sessionToken = token;
    response.packetData.udpPort = m_UDPPort;
    response.packetData.serverTimeMs = GetTickCount64();
    Net::PacketData packet;
    if (!Net::PacketCodec::EncodeTcp(response, packet))
        return;
    client.QueueSend(packet);
    m_vClients.push_back(std::move(client));
}
