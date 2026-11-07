#pragma once
#include "../Shared/NetworkMessages.h"
#include <cstddef>
#include <queue>
#include <vector>

// 소켓·UDP 주소 보관은 접속 처리 구현 시 추가한다.
class Client
{
public:
    Client();
    ~Client();
    Client(Client&& other) noexcept;
    Client& operator=(Client&& other) noexcept;

    //소켓 닫기
    void Close();

    //큐에 패킷 등록
    void QueueSend(Net::PacketData& data);
    //큐의 패킷을 실제로 보내고, 송신 완료한 패킷은 삭제
    void FlushSend();

    // 접속 소켓의 소유권을 넘겨받고 발급한 토큰을 저장한다.
    void SetConnection(SOCKET socket, const Net::SessionToken& token)
    {
        Close();
        m_TCPSocket = socket;
        m_sessionToken = token;
    }
    SOCKET GetTCPSocket() const { return m_TCPSocket; }
    const Net::SessionToken& GetSessionToken() const { return m_sessionToken; }
    bool HasPendingSend() const { return !m_qPendingSend.empty(); }
    bool IsUDPRegistered() const { return m_bUDPRegistered; }
    const sockaddr_in& GetUDPAddress() const { return m_UDPAddress; }

private:
    Net::SessionToken           m_sessionToken{};

    SOCKET                      m_TCPSocket = INVALID_SOCKET;
    sockaddr_in                 m_UDPAddress{};
    bool                        m_bUDPRegistered = false;

    Net::RoomId                 m_nRoomId = 0;
    Net::MembershipId           m_nMembershipId = 0;

    // 이 클라가 보내온 TCP 데이터 저장하는 버퍼
    std::vector<std::uint8_t>   m_vTcpReceiveBuffer;
    // 이 클라에게 보낼 패킷 데이터 모음
    std::queue<Net::PacketData> m_qPendingSend;
    // 현재 송신중인 패킷을 몇 바이트만큼 보냈는지
    std::size_t                 m_nSendOffset = 0;

    // 0은 미입장 상태로 두고, 첫 입장에는 1부터 발급
    //멤버십ID는, 클라가 같은 방을 나갔다 들어왔을 때,
    //나가기 전에 송신한 패킷 정보를 구분하고 처리하지 않도록 하기 위함
    Net::MembershipId           m_nNextMembershipId = 1;

    // 첫 UDP 등록 전에는 기준 순번이 없음
    //아래는 UDP 주소를 접속 도중 변경하는 경우를 위해 존재함
    //우리 프로젝트 설계상 게임 접속 = UDP 등록이므로 사용할 필요 없음
    Net::Sequence lastRegistrationSequence = 0;
    bool hasRegistrationSequence = false;
};
