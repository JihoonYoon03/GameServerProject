#include "pch.h"
#include "NetworkClient.h"

void NetworkClient::RegisterUDP()
{
	Net::UdpHeader UDPHeader;
	UDPHeader.type = Net::UdpType::RegisterRequest;
	UDPHeader.sessionToken = m_SessionToken;
	UDPHeader.size = sizeof(UDPHeader);

	//UDP소켓으로 서버에 send
	int retval = sendto(
		m_UDPSocket,
		(char*)&UDPHeader,
		sizeof(UDPHeader),
		0,
		(sockaddr*)&m_ServerAddr,
		sizeof(m_ServerAddr)
	);

	if (retval == SOCKET_ERROR) err_quit("Failed to send Register Request");
}

void NetworkClient::CompleteConnection(const Net::UdpRegisterResult& result)
{
	//등록 성공
	m_UDPRegistered.store(true);
}

//아래 네트워크 쓰레드 함수에서 호출됨
bool NetworkClient::StartNetwork()
{
	//소켓 생성
	m_TCPSocket = socket(AF_INET, SOCK_STREAM, 0);
	m_UDPSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (m_TCPSocket == INVALID_SOCKET || m_UDPSocket == INVALID_SOCKET) err_quit("Invalid TCP or UDP Socket");

	//서버 주소 sockaddr_in 구조체에 정보 저장
	memset(&m_ServerAddr, 0, sizeof(m_ServerAddr));
	m_ServerAddr.sin_family = AF_INET;
	//서버 주소는 나중에 게임 시작 시 입력받는 것으로 변경
	inet_pton(AF_INET, "127.0.0.1", &m_ServerAddr.sin_addr);
	//포트번호 나중에 Net헤더에 따로 작성해두기
	std::uint16_t port = 9000;
	m_ServerAddr.sin_port = htons(port);

	//TCP 접속 함수 호출
	if (!ConnectServer("127.0.0.1", port)) err_quit("Cannot connect to server!");
	//UDP 통신 등록 함수 호출
	RegisterUDP();

	//네트워크 쓰레드 시작
	m_hNetworkThread = CreateThread(NULL, 0, NetworkProc, (LPVOID)this, NULL, NULL);

	if (m_hNetworkThread == nullptr) return false;

	return true;
}

void NetworkClient::NetworkLoop()
{
	char buffer[Net::MaxUdpPacketBytes];

	//load로 원자적 실행
	//while문 계속 호출되지 않게 수정 필요
	while (!m_bStopRequested.load())
	{
		sockaddr_in sender{};
		int senderLen = sizeof(sender);
		int recved = recvfrom(
			m_UDPSocket,
			buffer,
			sizeof(buffer),
			0,
			(sockaddr*)&sender,
			&senderLen
		);
		if (recved == SOCKET_ERROR)
		{
			err_display("Failed to receive Packet Data");
			continue;
		}

		Net::PacketData packet;
		packet.transport = Net::PacketTransport::Udp;
		packet.bytes.assign(buffer, buffer + recved);

		if (sender.sin_addr.s_addr != m_ServerAddr.sin_addr.s_addr ||
			sender.sin_port != m_ServerAddr.sin_port)
			continue;

		//패킷 데이터 사이즈가 일치하는지 확인
		//패킷 데이터랑 헤더랑 같이 송수신하게 구조체 선언 해야할듯
		if (packet.bytes.size() == sizeof(Net::UdpRegisterResult))
		{
			//형변환해서 연결 완료 처리
			Net::UdpRegisterResult result;
			::memcpy(&result, packet.bytes.data(), sizeof(result));

			if (result.result != Net::ErrorCode::Ok)
			{
				//실패 처리. 재접속 등
				continue;
			}

			CompleteConnection(result);
		}
	}
}

DWORD WINAPI NetworkClient::NetworkProc(LPVOID param)
{
	NetworkClient* client = static_cast<NetworkClient*>(param);
	client->NetworkLoop();
	return 0;
}
