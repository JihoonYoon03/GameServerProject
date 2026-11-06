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

    other.m_sessionToken.fill(0);
    other.m_TCPSocket = INVALID_SOCKET;
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

        other.m_sessionToken.fill(0);
        other.m_TCPSocket = INVALID_SOCKET;
	}

	return *this;
}

void Client::Close()
{
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
