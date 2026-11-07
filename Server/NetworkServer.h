#pragma once
#include "../Shared/NetworkMessages.h"
#include "Client.h"
#include "RoomManager.h"

struct sockaddr_in;

class RoomManager;

class NetworkServer
{
public:
	NetworkServer();
	~NetworkServer();
	NetworkServer(const NetworkServer&) = delete;
	NetworkServer& operator=(const NetworkServer&) = delete;
	void SetRoomManager(RoomManager* manager) { m_pRoomManager = manager; }
	static DWORD WINAPI NetworkProc(LPVOID parameter);
	void FlushSendQueue();
	void SendTcpPending(Client& client);
	bool StartNetwork(std::uint16_t tcpPort, std::uint16_t udpPort);
	void NetworkLoop();
	void AcceptPlayer();

	// 토큰과 실제 수신 주소를 연결한다. 토큰만으로 발신자를 인증하지 않는다.
	void RegisterPlayerUDP(const Net::UdpHeader& header, const sockaddr_in& sourceAddress);

	void OnClientDisconnected(const Net::SessionToken& m_sessionToken);
	// 확인된 발신자와 패킷을 방 명령 큐로 전달하며 게임 상태는 직접 바꾸지 않는다.
	void DispatchPacket(const Net::SessionToken& verifiedSender,
		const Net::PacketData& packet);
	// 방 스레드의 송신 요청을 큐에 넣고 실제 소켓 송신은 통신 스레드가 담당한다.
	bool SendPacket(const Net::SendRequest& request);
	void ReplyTimeProbe(const Net::SessionToken& recipient,
		const Net::TimeProbeRequest& request, Net::ServerTimeMs receivedAtMs);
	void StopNetwork();

private:
	std::uint64_t m_ServerStartedAt = 0;
	std::uint64_t m_NextSessionId = 1;
	bool m_bWinsockStarted = false;
	std::uint16_t m_UDPPort = 0;
	SOCKET							m_SockListen = INVALID_SOCKET;
	SOCKET							m_SockUDP = INVALID_SOCKET;

	//클라이언트 목록
	std::vector<Client>				m_vClients;
	//통신 쓰레드 핸들
	HANDLE							m_hNetworkThread = nullptr;

	//클라에 보낼 UDP 패킷 저장하는 큐. 방이 넣고 통신 쓰레드가 꺼내 씀
	std::queue<Net::SendRequest>	m_qSendQueue;
	//큐를 여러 쓰레드에서 사용하므로 보호 필요
	CRITICAL_SECTION				m_CSSendQueue;

	RoomManager*					m_pRoomManager = nullptr;
	
	//통신 쓰레드 종료 요청 여부. StopNetwork에서 호출, 원자적으로 값 변경됨
	std::atomic<bool>				m_bStopRequested{true};
};