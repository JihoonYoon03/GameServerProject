#include "pch.h"
#include "NetworkServer.h"
#include "../Shared/PacketCodec.h"
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

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

NetworkServer::NetworkServer() : m_ControlThreadId(GetCurrentThreadId())
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
	// 요청을 복사하다 예외가 발생해도 다른 스레드가 큐를 사용할 수 있어야 한다.
	try
	{
		m_qSendQueue.push(request);
	}
	catch (...)
	{
		LeaveCriticalSection(&m_CSSendQueue);
		throw;
	}
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
	// Client가 보관한 부분 송신 위치부터 이어서 전송한다.
	if (client.GetTCPSocket() != INVALID_SOCKET && client.HasPendingSend())
		client.FlushSend();
}


bool NetworkServer::StartNetwork(std::uint16_t tcpPort, std::uint16_t udpPort)
{
	if (GetCurrentThreadId() != m_ControlThreadId || m_bCleanupInProgress || m_hNetworkThread != nullptr)
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
	DWORD expectedThreadId = 0;
	if (!server->m_NetworkThreadId.compare_exchange_strong(expectedThreadId, GetCurrentThreadId()))
		return 0;
	server->NetworkLoop();
	server->m_NetworkThreadId = 0;
	return 0;
}

bool NetworkServer::SetCommandSink(const std::function<void(const Net::RoomCommand&)>& sink)
{
	if (GetCurrentThreadId() != m_ControlThreadId || m_bCleanupInProgress || m_hNetworkThread != nullptr)
		return false;
	m_commandSink = sink;
	return true;
}

void NetworkServer::NotifyCommand(const Net::RoomCommand& command)
{
	if (!m_commandSink)
		return;

	// 방 명령 처리 중에는 같은 서버의 분배와 연결 정리를 다시 호출하지 않는다.
	m_bInvokingCommandSink = true;
	try
	{
		m_commandSink(command);
	}
	catch (...)
	{
		fprintf(stderr, "방 명령 콜백 처리 중 오류가 발생했습니다.\n");
	}
	m_bInvokingCommandSink = false;
}

bool NetworkServer::ReceiveTcp(Client& client)
{
	const SOCKET socketHandle = client.GetTCPSocket();
	if (socketHandle == INVALID_SOCKET)
		return false;

	std::uint8_t bytes[4096];
	const int received = recv(socketHandle, reinterpret_cast<char*>(bytes), static_cast<int>(sizeof(bytes)), 0);
	if (received == 0)
		return false;
	if (received == SOCKET_ERROR)
		return WSAGetLastError() == WSAEWOULDBLOCK;

	Net::SessionToken sender{};
	if (!ValidateSender(socketHandle, sender))
		return false;

	auto& buffer = client.m_vTcpReceiveBuffer;
	buffer.insert(buffer.end(), bytes, bytes + received);
	std::size_t offset = 0;
	while (offset < buffer.size() && !m_bStopRequested)
	{
		Net::TcpHeader header;
		const Net::DecodeStatus status = Net::PacketCodec::ReadTcpHeader(
			buffer.data() + offset, buffer.size() - offset, header);
		if (status == Net::DecodeStatus::NeedMore)
			break;
		if (status == Net::DecodeStatus::Invalid)
			return false;

		Net::PacketData packet;
		packet.transport = Net::PacketTransport::Tcp;
		const auto first = buffer.begin() + static_cast<std::ptrdiff_t>(offset);
		packet.bytes.assign(first, first + header.size);
		DispatchPacket(sender, packet);
		offset += header.size;
	}
	// 처리한 프레임만 지우고 뒤에 이어지는 미완성 데이터는 보관한다.
	buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(offset));
	return true;
}

void NetworkServer::ReceiveUdp()
{
	std::uint8_t bytes[Net::MaxUdpPacketBytes];
	sockaddr_in sourceAddress{};
	int addressLength = sizeof(sourceAddress);
	const int received = recvfrom(m_SockUDP, reinterpret_cast<char*>(bytes), static_cast<int>(sizeof(bytes)), 0,
		reinterpret_cast<sockaddr*>(&sourceAddress), &addressLength);
	if (received <= 0)
		return;

	Net::UdpHeader header;
	if (!Net::PacketCodec::ReadUdpHeader(bytes, static_cast<std::size_t>(received), header))
		return;

	if (header.type == Net::UdpType::RegisterRequest)
	{
		Net::UdpRegisterRequest request;
		if (Net::PacketCodec::DecodeBody(header.type, bytes + Net::UdpHeaderBytes,
			header.size - Net::UdpHeaderBytes, request))
			RegisterPlayerUDP(header, sourceAddress);
		return;
	}

	Net::SessionToken sender{};
	if (!ValidateSender(header, sourceAddress, sender))
		return;

	Net::PacketData packet;
	packet.transport = Net::PacketTransport::Udp;
	packet.bytes.assign(bytes, bytes + received);
	DispatchPacket(sender, packet);
}

void NetworkServer::DispatchPacket(const Net::SessionToken& verifiedSender, const Net::PacketData& packet)
{
	if (GetCurrentThreadId() != m_NetworkThreadId || m_bInvokingCommandSink || m_bStopRequested)
		return;

	const Client* sender = nullptr;
	for (const Client& client : m_vClients)
	{
		if (client.GetTCPSocket() != INVALID_SOCKET && client.GetSessionToken() == verifiedSender)
		{
			sender = &client;
			break;
		}
	}
	if (sender == nullptr)
		return;

	Net::RoomCommand command;
	command.sessionToken = verifiedSender;
	command.context = { sender->GetRoomId(), sender->GetMatchId() };
	command.membershipId = sender->GetMembershipId();
	command.receivedAtMs = GetTickCount64();

	const auto* bytes = packet.bytes.data();
	const std::size_t size = packet.bytes.size();
	if (packet.transport == Net::PacketTransport::Tcp)
	{
		Net::TcpHeader header;
		if (Net::PacketCodec::ReadTcpHeader(bytes, size, header) != Net::DecodeStatus::Complete ||
			header.size != size)
			return;

		const auto* body = bytes + Net::TcpHeaderBytes;
		const std::size_t bodySize = size - Net::TcpHeaderBytes;
		command.requestId = header.requestId;
		if (header.type == Net::TcpType::TimeProbeRequest)
		{
			Net::TimeProbeRequest request;
			if (Net::PacketCodec::DecodeBody(header.type, body, bodySize, request))
				ReplyTimeProbe(verifiedSender, request, command.receivedAtMs, header.requestId);
			return;
		}
		if (!m_commandSink || command.context.roomId == 0 || command.membershipId == 0)
			return;

		switch (header.type)
		{
		case Net::TcpType::LeaveRoomRequest:
		{
			Net::LeaveRoomRequest request;
			if (!Net::PacketCodec::DecodeBody(header.type, body, bodySize, request) ||
				request.roomId != command.context.roomId)
				return;
			command.body = request;
			break;
		}
		case Net::TcpType::MapLoaded:
		{
			Net::MapLoaded request;
			if (!Net::PacketCodec::DecodeBody(header.type, body, bodySize, request) ||
				request.roomId != command.context.roomId)
				return;
			command.body = request;
			break;
		}
		case Net::TcpType::ReadyChangeRequest:
		{
			Net::ReadyChangeRequest request;
			if (!Net::PacketCodec::DecodeBody(header.type, body, bodySize, request) ||
				request.roomId != command.context.roomId)
				return;
			command.body = request;
			break;
		}
		case Net::TcpType::FireRequest:
		{
			Net::FireRequest request;
			if (!Net::PacketCodec::DecodeBody(header.type, body, bodySize, request) ||
				request.context.roomId != command.context.roomId || command.context.matchId == 0 ||
				request.context.matchId != command.context.matchId || request.membershipId != command.membershipId)
				return;
			command.body = request;
			break;
		}
		case Net::TcpType::ReloadRequest:
		{
			Net::ReloadRequest request;
			if (!Net::PacketCodec::DecodeBody(header.type, body, bodySize, request) ||
				request.context.roomId != command.context.roomId || command.context.matchId == 0 ||
				request.context.matchId != command.context.matchId || request.membershipId != command.membershipId)
				return;
			command.body = request;
			break;
		}
		default:
			return;
		}
	}
	else if (packet.transport == Net::PacketTransport::Udp)
	{
		if (!m_commandSink || command.context.roomId == 0 || command.context.matchId == 0 ||
			command.membershipId == 0)
			return;
		Net::UdpHeader header;
		if (!Net::PacketCodec::ReadUdpHeader(bytes, size, header) ||
			header.type != Net::UdpType::PlayerTransform || header.m_sessionToken != verifiedSender ||
			header.context.roomId != command.context.roomId || header.context.matchId != command.context.matchId ||
			header.membershipId != command.membershipId)
			return;

		Net::PlayerTransform transform;
		if (!Net::PacketCodec::DecodeBody(header.type, bytes + Net::UdpHeaderBytes,
			size - Net::UdpHeaderBytes, transform))
			return;
		command.body = transform;
	}
	else
		return;

	NotifyCommand(command);
}

void NetworkServer::OnClientDisconnected(const Net::SessionToken& sessionToken)
{
	const DWORD threadId = GetCurrentThreadId();
	if (threadId != m_NetworkThreadId && (threadId != m_ControlThreadId || !m_bCleanupInProgress))
		return;
	if (m_bInvokingCommandSink)
		return;

	for (auto client = m_vClients.begin(); client != m_vClients.end(); ++client)
	{
		if (client->GetSessionToken() != sessionToken)
			continue;

		Net::RoomCommand command;
		command.sessionToken = client->GetSessionToken();
		command.context = { client->GetRoomId(), client->GetMatchId() };
		command.membershipId = client->GetMembershipId();
		command.receivedAtMs = GetTickCount64();
		command.body = Net::ClientDisconnected{};
		m_vClients.erase(client);

		if (command.context.roomId != 0 && command.membershipId != 0)
			NotifyCommand(command);
		return;
	}
}

void NetworkServer::NetworkLoop()
{
	if (GetCurrentThreadId() != m_NetworkThreadId || m_bInvokingCommandSink)
		return;
	while (!m_bStopRequested)
	{
		FlushSendQueue();
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(m_SockListen, &readSet);
		FD_SET(m_SockUDP, &readSet);

		// 짧게 대기해 송신 큐와 종료 요청을 계속 확인한다.
		timeval timeout{};
		timeout.tv_usec = 10000;
		const int ready = select(0, &readSet, nullptr, nullptr, &timeout);
		if (ready == SOCKET_ERROR)
		{
			err_display("select()");
			m_bStopRequested = true;
			break;
		}
		if (m_bStopRequested)
			break;
		if (FD_ISSET(m_SockListen, &readSet))
			AcceptPlayer();
		if (FD_ISSET(m_SockUDP, &readSet))
			ReceiveUdp();

		// 연결된 소켓은 비차단으로 읽으므로 연결 수가 select의 집합 한도를 넘더라도 읽을 수 있다.
		for (std::size_t i = 0; i < m_vClients.size() && !m_bStopRequested;)
		{
			Client& client = m_vClients[i];
			if (!ReceiveTcp(client))
			{
				const Net::SessionToken token = client.GetSessionToken();
				OnClientDisconnected(token);
				continue;
			}
			if (m_bStopRequested)
				break;
			SendTcpPending(client);
			++i;
		}
	}
}

void NetworkServer::StopNetwork()
{
	EnterCriticalSection(&m_CSSendQueue);
	m_bStopRequested = true;
	LeaveCriticalSection(&m_CSSendQueue);

	// 통신 스레드나 정리 중인 콜백에서는 종료 요청만 남긴다.
	if (GetCurrentThreadId() != m_ControlThreadId || m_bCleanupInProgress)
		return;
	m_bCleanupInProgress = true;
	if (m_hNetworkThread != nullptr)
	{
		WaitForSingleObject(m_hNetworkThread, INFINITE);
		CloseHandle(m_hNetworkThread);
		m_hNetworkThread = nullptr;
	}
	// 통신 스레드가 끝난 뒤 남은 세션을 같은 경로로 정리한다.
	while (!m_vClients.empty())
	{
		const Net::SessionToken token = m_vClients.back().GetSessionToken();
		OnClientDisconnected(token);
	}
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
	m_bCleanupInProgress = false;
}


void NetworkServer::AcceptPlayer()
{
	if (GetCurrentThreadId() != m_NetworkThreadId || m_bInvokingCommandSink)
		return;

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
	if (ioctlsocket(socketHandle, FIONBIO, &nonblocking) == SOCKET_ERROR)
	{
		closesocket(socketHandle);
		return;
	}

	// 운영체제 난수로 같은 주소의 다른 접속도 추측할 수 없는 토큰을 만든다.
	Net::SessionToken token{};
	const NTSTATUS result = BCryptGenRandom(nullptr, token.data(), static_cast<ULONG>(token.size()),
		BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	if (result < 0 || !Net::IsValidSessionToken(token))
	{
		closesocket(socketHandle);
		return;
	}
	for (const Client& connected : m_vClients)
	{
		if (connected.GetSessionToken() == token)
		{
			closesocket(socketHandle);
			return;
		}
	}

	Client client;
	client.SetConnection(socketHandle, token);
	Net::ConnectResult response;
	response.sessionToken = token;
	response.udpPort = m_UDPPort;
	response.serverTimeMs = GetTickCount64();
	Net::PacketData packet;
	if (!Net::PacketCodec::EncodeTcp(response, 0, packet))
		return;
	client.QueueSend(packet);
	m_vClients.push_back(std::move(client));
}
