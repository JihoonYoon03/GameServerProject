#include "pch.h"
#include "NetworkServer.h"

void NetworkServer::RegisterPlayerUDP(const Net::UdpHeader& header, const sockaddr_in& sourceAddress)
{
	int retval = 0;
	//헤더 정보 확인은 NetworkLoop에서 처리할 예정
	
	//AcceptPlayer에서 클라이언트 TCP 접속을 받고, 토큰 저장한 뒤 클라에게 알림
	//UDP 연결 시도에서 해당 토큰을 클라에서 보내옴
	for (Client& client : m_vClients)
	{
		//세션 토큰 불일치 클라는 넘기기
		if (client.sessionToken != header.sessionToken)
			continue;

		Net::UdpRegisterResult regResult;
		//시퀀스는 패킷 순번을 의미함. 해당 값으로 어떤 요청 답변이 들어왔는지 확인 가능
		regResult.registrationSequence = header.sequence;

		sockaddr_in tcpPeer{};
		int addrlen = sizeof(sockaddr_in);
		retval = getpeername(client.TCPSocket, (sockaddr*)&tcpPeer, &addrlen);

		// 소켓 에러
		if (retval == SOCKET_ERROR)
		{
			err_display("getpeername()");
			regResult.result = Net::ErrorCode::UdpNotRegistered;
		}
		// UDP 연결 시도한 클라의 주소체계가 다르거나
		// 토큰이 같은 클라이언트의 TCP IP주소와 다른 경우
		else if (sourceAddress.sin_family != AF_INET ||
				tcpPeer.sin_addr.s_addr != sourceAddress.sin_addr.s_addr)
		{
			regResult.result = Net::ErrorCode::UdpNotRegistered;
		}
		else
		{

			client.UDPAddress = sourceAddress;
			client.UDPRegistered = true;
		}

		retval = sendto(
			m_SockUDP,
			(char*)&regResult,
			sizeof(regResult),
			0,
			(sockaddr*)&sourceAddress,
			sizeof(sourceAddress)
		);
		if (retval == SOCKET_ERROR)	err_display("sendto()");

		return;
	}
}
