#include "pch.h"
#include "NetworkServer.h"
#include "../Shared/PacketCodec.h"

//UDP 주소 2개가 같은지 여부
bool SameEndpoint(const sockaddr_in& first, const sockaddr_in& second)
{
	return first.sin_family == AF_INET && second.sin_family == AF_INET &&
		first.sin_addr.s_addr == second.sin_addr.s_addr &&
		first.sin_port == second.sin_port;
}

void NetworkServer::RegisterPlayerUDP(const Net::UdpHeader& header, const sockaddr_in& sourceAddress)
{
	//서버 종료 요청 상태인지 || 헤더가 정상이 아닌지 ||
	//헤더 타입이 등록 요청이 아닌지 || 헤더 외에 다른 데이터가 있는지 ||
	//헤더 변수 중, 등록 요청과 상관없는 값이 기본값이 아닌지 ||
	//접속 요청한 클라 주소가 IPv4가 아닌지 || 포트가 0으로 되어있는지
	if (m_bStopRequested || !Net::PacketCodec::ValidateHeader(header) ||
		header.type != Net::UdpType::RegisterRequest || header.size != Net::UdpHeaderBytes ||
		header.context.roomId != 0 || header.context.matchId != 0 ||
		header.membershipId != 0 || header.serverTick != 0 ||
		sourceAddress.sin_family != AF_INET || sourceAddress.sin_port == 0)
		return;

	//TCP로 접속한 클라이언트 목록 순회
	for (Client& client : m_vClients)
	{
		//소켓 없거나 토큰 불일치 시 스킵
		if (client.GetTCPSocket() == INVALID_SOCKET || client.GetSessionToken() != header.m_sessionToken)
			continue;

		//TCP 소켓에서 주소 가져오고
		//해당 주소와 UDP 접속 요청한 클라이언트 주소를 비교
		sockaddr_in tcpPeer{};
		int length = sizeof(tcpPeer);
		if (getpeername(client.GetTCPSocket(), (sockaddr*)(&tcpPeer), &length) == SOCKET_ERROR ||
			tcpPeer.sin_family != AF_INET || tcpPeer.sin_addr.s_addr != sourceAddress.sin_addr.s_addr)
			return;

		//만약 UDP가 이미 등록 된 클라이고 2개의 주소가 같은 경우 갱신하지 않음.
		if (client.m_bUDPRegistered && !SameEndpoint(client.m_UDPAddress, sourceAddress))
			return;

		//최초 요청이 아님 && 등록 요청 순번이 다름 && 지금 처리하는 요청이 새로운 요청이 아닌 경우
		//갱신하지 않음. 이외는 새로 등록 or 등록 재시도로 판단되어 계속 진행
		if (client.hasRegistrationSequence && header.sequence != client.lastRegistrationSequence &&
			!Net::IsNewerSequence(header.sequence, client.lastRegistrationSequence))
			return;

		//등록에 대한 응답 데이터 생성
		Net::UdpPacket<Net::UdpRegisterResult> response;
		response.header.type = Net::UdpType::RegisterResult;
		//UdpHeader 바이트수 상수 + UdpRegisterResult 멤버 변수 바이트 수
		//sizeof()시 바이트 패딩이 적용되어 상수값으로 더한 것인데
		//패킷 구조체를 pragma pack 적용하면 sizeof로 써도 됨.
		response.header.size = Net::UdpHeaderBytes + 2 + 4;
		response.header.m_sessionToken = client.GetSessionToken();
		//UDP 통신 순번
		response.header.sequence = header.sequence;
		//등록 요청 순번
		response.packetData.registrationSequence = header.sequence;
		Net::SendRequest send;
		send.recipientToken = client.GetSessionToken();
		//직렬화(빅 엔디언으로 연속된 바이트 공간에 정렬)
		if (!Net::PacketCodec::EncodeUdp(response, send.packet))
			return;
		
		//클라이언트 객체에 정보 등록 후 큐에 패킷 푸쉬하기
		client.m_UDPAddress = sourceAddress;
		client.m_bUDPRegistered = true;
		client.lastRegistrationSequence = header.sequence;
		client.hasRegistrationSequence = true;
		SendPacket(send);
		return;
	}
}

bool NetworkServer::ValidateSender(SOCKET tcpSocket, Net::SessionToken& verifiedSender) const
{
	if (tcpSocket == INVALID_SOCKET)
		return false;
	for (const Client& client : m_vClients)
	{
		if (client.GetTCPSocket() == tcpSocket)
		{
			verifiedSender = client.GetSessionToken();
			return true;
		}
	}
	return false;
}

bool NetworkServer::ValidateSender(const Net::UdpHeader& header, const sockaddr_in& sourceAddress,
	Net::SessionToken& verifiedSender)
{
	//메시지 종류(PlayerTransform)와 크기 확인
	if (!Net::PacketCodec::ValidateHeader(header) || header.type != Net::UdpType::PlayerTransform ||
		header.size != Net::UdpHeaderBytes + 4 + 64 || header.serverTick != 0)
		return false;
	for (Client& client : m_vClients)
	{
		//UDP Sender와 동일한 클라이언트인지 확인
		if (client.GetTCPSocket() == INVALID_SOCKET || client.GetSessionToken() != header.m_sessionToken ||
			!client.m_bUDPRegistered || !SameEndpoint(client.m_UDPAddress, sourceAddress))
			continue;

		//Sender가 보낸 헤더의 정보와 현재 클라이언트의 정보가 매칭되는지 확인
		if (client.m_nRoomId == 0 || client.m_nMatchId == 0 || client.m_nMembershipId == 0 ||
			header.context.roomId != client.m_nRoomId || header.context.matchId != client.m_nMatchId ||
			header.membershipId != client.m_nMembershipId ||
			(client.m_HasUdpSequence && !Net::IsNewerSequence(header.sequence, client.m_LastUdpSequence)))
			return false;

		//마지막 순번 기억하고, 첫 UDP 통신 완료했음을 설정함
		client.m_LastUdpSequence = header.sequence;
		client.m_HasUdpSequence = true;
		verifiedSender = client.GetSessionToken();
		return true;
	}
	return false;
}

//recipient = 검증 끝난 클라이언트의 토큰
void NetworkServer::ReplyTimeProbe(const Net::SessionToken& recipient,
	const Net::TimeProbeRequest& request, Net::ServerTimeMs receivedAtMs, std::uint32_t requestId)
{
	Net::TcpPacket<Net::TimeProbeResult> response;
	response.header.type = Net::TcpType::TimeProbeResult;
	//사이즈 계산
	response.header.size = Net::TcpHeaderBytes + 8 + 8 + 8;
	//TCP 통신 순번
	response.header.requestId = requestId;
	//클라이언트가 요청했을 때의 시각 그대로 보내줌
	response.packetData.clientSendMs = request.clientSendMs;
	//서버가 이 요청을 받았을 때의 서버 시각
	response.packetData.serverReceiveMs = receivedAtMs;
	//서버가 요청을 받고 다시 보냈을 때의 서버 시각 
	response.packetData.serverSendMs = GetTickCount64();

	//서버가 요청 받은 시각보다 다시 보낼때 시각이 더 빠른 경우는 존재하면 안 됨
	if (receivedAtMs > response.packetData.serverSendMs)
		return;

	Net::SendRequest send;
	send.recipientToken = recipient;
	if (Net::PacketCodec::EncodeTcp(response, send.packet))
		SendPacket(send);
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
