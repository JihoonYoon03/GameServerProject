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
	// 생성한 제어 스레드에서 시작과 정리, 소멸을 수행한다.
	bool StartNetwork(std::uint16_t tcpPort, std::uint16_t udpPort);
	void NetworkLoop();
	// 제어 스레드에서 시작 전에 지정한다. 종료가 끝날 때까지 콜백이 참조하는 객체를 유지한다.
	// 콜백은 통신 스레드와 정리 중인 제어 스레드에서 호출된다. 명령 참조는 호출 중에만 유효하다.
	bool SetCommandSink(const std::function<void(const Net::RoomCommand&)>& sink);
	void AcceptPlayer();

	//Player의 UDP 접속 요청 처리 함수
	void RegisterPlayerUDP(const Net::UdpHeader& header, const sockaddr_in& sourceAddress);

	//소켓을 통해 세션 토큰 찾는 함수
    bool ValidateSender(SOCKET tcpSocket, Net::SessionToken& verifiedSender) const;
	//UDP용 ValidateSender
    bool ValidateSender(const Net::UdpHeader& header, const sockaddr_in& sourceAddress,
        Net::SessionToken& verifiedSender);

	void OnClientDisconnected(const Net::SessionToken& m_sessionToken);
	// 확인된 발신자와 패킷을 방 명령 큐로 전달하며 게임 상태는 직접 바꾸지 않는다.
	void DispatchPacket(const Net::SessionToken& verifiedSender,
		const Net::PacketData& packet);
	// 방 스레드의 송신 요청을 큐에 넣고 실제 소켓 송신은 통신 스레드가 담당한다.
	bool SendPacket(const Net::SendRequest& request);

    //클라-서버간 시각 동기화를 위한 함수
	//((서버 수신 시각 - 클라 송신 시각) + (서버 송신 시각 - 클라 수신 시각)) / 2 로 계산
	//통신이 왕복할 때 두 방향의 지연이 서로 같다고 가정함.
	//결과값으로 서버 시계가 클라보다 얼마나 앞서는지 측정 가능함.
	//이 값을 저장해 클라 시간값에 오프셋으로 더하면 됨.
	//통신 시에 지연 시간을 측정하기 위한 함수로 대략적으로 사용할 수도 있음.
	void ReplyTimeProbe(const Net::SessionToken& recipient,
		const Net::TimeProbeRequest& request, Net::ServerTimeMs receivedAtMs,
        std::uint32_t requestId = 0);

	// 통신 스레드에서는 종료를 요청만 한다. 제어 스레드가 다시 호출해 스레드와 소켓을 정리한다.
	void StopNetwork();

private:
#ifdef SERVER_NETWORK_TEST
	friend class NetworkServerTestAccess;
#endif
	bool ReceiveTcp(Client& client);
	void ReceiveUdp();
	void NotifyCommand(const Net::RoomCommand& command);
	std::function<void(const Net::RoomCommand&)> m_commandSink;
	const DWORD m_ControlThreadId;
	std::atomic<DWORD> m_NetworkThreadId{0};
	bool m_bInvokingCommandSink = false;
	bool m_bCleanupInProgress = false;
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