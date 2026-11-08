#include "pch.h"
#include "Client.h"

Client::Client()
{
}

Client::~Client()
{
    Close();
}

Client::Client(Client && other) noexcept
{
    //본인의 소켓 닫기
    Close();
    m_sessionToken = other.m_sessionToken;
    m_TCPSocket = std::move(other.m_TCPSocket);
    m_UDPAddress = other.m_UDPAddress;
    m_bUDPRegistered = other.m_bUDPRegistered;
    m_nRoomId = other.m_nRoomId;
    m_nMembershipId = other.m_nMembershipId;
    m_vTcpReceiveBuffer = std::move(other.m_vTcpReceiveBuffer);
    m_qPendingSend = std::move(other.m_qPendingSend);
    m_nSendOffset = other.m_nSendOffset;
    m_nNextMembershipId = other.m_nNextMembershipId;
    m_nMatchId = other.m_nMatchId;
    lastRegistrationSequence = other.lastRegistrationSequence;
    hasRegistrationSequence = other.hasRegistrationSequence;
    m_LastUdpSequence = other.m_LastUdpSequence;
    m_HasUdpSequence = other.m_HasUdpSequence;

    other.m_TCPSocket = INVALID_SOCKET;
    other.Close();
}

Client& Client::operator=(Client && other) noexcept
{
	if (this != &other)
	{
        this->Close();
        m_sessionToken = other.m_sessionToken;
        m_TCPSocket = std::move(other.m_TCPSocket);
        m_UDPAddress = other.m_UDPAddress;
        m_bUDPRegistered = other.m_bUDPRegistered;
        m_nRoomId = other.m_nRoomId;
        m_nMembershipId = other.m_nMembershipId;
        m_vTcpReceiveBuffer = std::move(other.m_vTcpReceiveBuffer);
        m_qPendingSend = std::move(other.m_qPendingSend);
        m_nSendOffset = other.m_nSendOffset;
        m_nNextMembershipId = other.m_nNextMembershipId;
        m_nMatchId = other.m_nMatchId;
        lastRegistrationSequence = other.lastRegistrationSequence;
        hasRegistrationSequence = other.hasRegistrationSequence;
        m_LastUdpSequence = other.m_LastUdpSequence;
        m_HasUdpSequence = other.m_HasUdpSequence;

        other.m_TCPSocket = INVALID_SOCKET;
        other.Close();
	}

	return *this;
}

void Client::Close()
{
    m_sessionToken.fill(0);
    m_UDPAddress = {};
    m_bUDPRegistered = false;
    m_nRoomId = 0;
    m_nMatchId = 0;
    m_nMembershipId = 0;
    lastRegistrationSequence = 0;
    hasRegistrationSequence = false;
    m_LastUdpSequence = 0;
    m_HasUdpSequence = false;
    m_vTcpReceiveBuffer.clear();
    m_qPendingSend = {};
    m_nSendOffset = 0;
    if (m_TCPSocket != INVALID_SOCKET)
    {
        int retval = closesocket(m_TCPSocket);
        if (retval == SOCKET_ERROR) err_display("Client::Close()");
        else m_TCPSocket = INVALID_SOCKET;
    }
}

void Client::QueueSend(Net::PacketData& data)
{
    m_qPendingSend.push(data);
}

void Client::FlushSend()
{
    while (!m_qPendingSend.empty())
    {
        Net::PacketData packet = m_qPendingSend.front();

        //얼마만큼 보냈는지 계속해서 기록
        int retval = send(
            m_TCPSocket, 
            (char*)&packet.bytes[m_nSendOffset],
            packet.bytes.size() - m_nSendOffset,
            0
        );
        if (retval == SOCKET_ERROR)
        {
            err_display("Client::FlushSend() send()");
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK)
            {
                //송신 버퍼에 여유 없음
                return;
            }
            else
            {
                //다른 에러 처리
            }
        }
         
        m_nSendOffset += retval;


        //데이터 전부 전송 완료 시, 오프셋 초기화 후 큐 맨 앞을 pop
        if (m_nSendOffset == packet.bytes.size())
        {
            m_nSendOffset = 0;
            m_qPendingSend.pop();
        }
    }
}

void Client::SetMatchContext(Net::MatchContext context, Net::MembershipId membershipId)
{
    //방이 다르다 || 다른 경기이다 || 방에 입장해서 부여받은 ID가 다른가(재접속 구분용)
    if (m_nRoomId != context.roomId || m_nMatchId != context.matchId ||
        m_nMembershipId != membershipId)
        //이동 데이터용 UDP 패킷을 0번부터 다시 받을 수 있도록 세팅
        m_HasUdpSequence = false;

    m_nRoomId = context.roomId;
    m_nMatchId = context.matchId;
    m_nMembershipId = membershipId;
}
