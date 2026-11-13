#include "pch.h"
#include "NetworkClient.h"
#include "../Shared/PacketCodec.h"

NetworkClient::NetworkClient() : m_ControlThreadId(GetCurrentThreadId())
{
    InitializeCriticalSection(&m_CSSendQueue);
    InitializeCriticalSection(&m_CSReceiveQueue);
}

NetworkClient::~NetworkClient()
{
    DisconnectServer();
    DeleteCriticalSection(&m_CSReceiveQueue);
    DeleteCriticalSection(&m_CSSendQueue);
}

bool NetworkClient::ConnectServer(const std::string& address, std::uint16_t tcpPort)
{
    if (GetCurrentThreadId() != m_ControlThreadId || m_hNetworkThread != nullptr
        || m_TCPSocket != INVALID_SOCKET || tcpPort == 0)
        return false;

    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        return false;
    m_WinsockStarted = true;
    m_ServerTcpAddr = {};
    m_ServerTcpAddr.sin_family = AF_INET;
    m_ServerTcpAddr.sin_port = htons(tcpPort);
    if (InetPtonA(AF_INET, address.c_str(), &m_ServerTcpAddr.sin_addr) != 1)
    {
        DisconnectServer();
        return false;
    }
    m_ServerUdpAddr = m_ServerTcpAddr;
    m_ServerUdpAddr.sin_port = 0;
    m_TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    m_UDPSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_TCPSocket == INVALID_SOCKET || m_UDPSocket == INVALID_SOCKET)
    {
        DisconnectServer();
        return false;
    }

    sockaddr_in localAddress{};
    localAddress.sin_family = AF_INET;
    localAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    u_long nonblocking = 1;
    if (bind(m_UDPSocket, reinterpret_cast<sockaddr*>(&localAddress), sizeof(localAddress)) == SOCKET_ERROR
        || ioctlsocket(m_TCPSocket, FIONBIO, &nonblocking) == SOCKET_ERROR
        || ioctlsocket(m_UDPSocket, FIONBIO, &nonblocking) == SOCKET_ERROR)
    {
        DisconnectServer();
        return false;
    }

    int result = connect(m_TCPSocket, reinterpret_cast<sockaddr*>(&m_ServerTcpAddr), sizeof(m_ServerTcpAddr));
    if (result == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
    {
        DisconnectServer();
        return false;
    }
    m_TcpConnecting = result == SOCKET_ERROR;
    m_bStopRequested = false;
    return true;
}

bool NetworkClient::StartNetwork()
{
    if (GetCurrentThreadId() != m_ControlThreadId || m_hNetworkThread != nullptr
        || m_bStopRequested || m_TCPSocket == INVALID_SOCKET || m_UDPSocket == INVALID_SOCKET)
        return false;
    m_hNetworkThread = CreateThread(nullptr, 0, NetworkProc, this, 0, nullptr);
    if (m_hNetworkThread == nullptr)
    {
        DisconnectServer();
        return false;
    }
    return true;
}

DWORD WINAPI NetworkClient::NetworkProc(LPVOID parameter)
{
    NetworkClient* client = static_cast<NetworkClient*>(parameter);
    client->m_NetworkThreadId = GetCurrentThreadId();
    try
    {
        client->NetworkLoop();
    }
    catch (...)
    {
        fputs("통신 처리 중 오류가 발생했습니다.\n", stderr);
    }
    client->m_bStopRequested = true;
    client->m_NetworkThreadId = 0;
    return 0;
}

void NetworkClient::RegisterUDP()
{
    if (GetCurrentThreadId() != m_NetworkThreadId || !m_HasConnectResult
        || m_RegistrationSent || m_bStopRequested)
        return;
    Net::UdpPacket<Net::UdpRegisterRequest> request;
    request.header.type = Net::UdpType::RegisterRequest;
    request.header.m_sessionToken = m_SessionToken;
    request.header.sequence = ++m_RegistrationSequence;
    Net::PacketData packet;
    if (!Net::PacketCodec::EncodeUdp(request, packet) || !SendPacket(packet))
    {
        m_bStopRequested = true;
        return;
    }
    m_RegistrationSent = true;
}

void NetworkClient::CompleteConnection(const Net::UdpRegisterResult& result)
{
    if (GetCurrentThreadId() != m_NetworkThreadId || !m_RegistrationSent
        || result.registrationSequence != m_RegistrationSequence || m_UDPRegistered)
        return;
    if (result.result != Net::ErrorCode::Ok)
        m_bStopRequested = true;
    else
        m_UDPRegistered = true;
}

bool NetworkClient::SendPacket(const Net::PacketData& packet)
{
    if (packet.transport == Net::PacketTransport::Tcp)
    {
        Net::TcpHeader header;
        if (!m_UDPRegistered || Net::PacketCodec::ReadTcpHeader(packet.bytes.data(), packet.bytes.size(), header)
            != Net::DecodeStatus::Complete || header.size != packet.bytes.size())
            return false;
    }
    else if (packet.transport == Net::PacketTransport::Udp)
    {
        Net::UdpHeader header;
        if (!Net::PacketCodec::ReadUdpHeader(packet.bytes.data(), packet.bytes.size(), header))
            return false;
        if (header.type == Net::UdpType::RegisterRequest)
        {
            // 등록 전 송신의 예외는 통신 스레드의 최초 등록 요청에만 둔다.
            if (GetCurrentThreadId() != m_NetworkThreadId || m_RegistrationSent)
                return false;
        }
        else if (!m_UDPRegistered)
            return false;
    }
    else
        return false;

    EnterCriticalSection(&m_CSSendQueue);
    try
    {
        if (m_bStopRequested)
        {
            LeaveCriticalSection(&m_CSSendQueue);
            return false;
        }
        m_qSendQueue.push(packet);
    }
    catch (...)
    {
        LeaveCriticalSection(&m_CSSendQueue);
        return false;
    }
    LeaveCriticalSection(&m_CSSendQueue);
    return true;
}

bool NetworkClient::FlushSendQueue()
{
    while (!m_bStopRequested)
    {
        if (!m_HasCurrentSend)
        {
            EnterCriticalSection(&m_CSSendQueue);
            if (m_qSendQueue.empty())
            {
                LeaveCriticalSection(&m_CSSendQueue);
                return true;
            }
            m_CurrentSend = std::move(m_qSendQueue.front());
            m_qSendQueue.pop();
            LeaveCriticalSection(&m_CSSendQueue);
            m_HasCurrentSend = true;
            m_SendOffset = 0;
        }

        int sent = 0;
        if (m_CurrentSend.transport == Net::PacketTransport::Tcp)
        {
            if (m_TcpConnecting)
                return true;
            sent = send(m_TCPSocket, reinterpret_cast<const char*>(m_CurrentSend.bytes.data() + m_SendOffset),
                static_cast<int>(m_CurrentSend.bytes.size() - m_SendOffset), 0);
        }
        else
        {
            sent = sendto(m_UDPSocket, reinterpret_cast<const char*>(m_CurrentSend.bytes.data()),
                static_cast<int>(m_CurrentSend.bytes.size()), 0,
                reinterpret_cast<sockaddr*>(&m_ServerUdpAddr), sizeof(m_ServerUdpAddr));
        }
        if (sent == SOCKET_ERROR)
            return WSAGetLastError() == WSAEWOULDBLOCK;
        if (sent <= 0)
            return false;
        if (m_CurrentSend.transport == Net::PacketTransport::Tcp)
            m_SendOffset += static_cast<std::size_t>(sent);
        else if (static_cast<std::size_t>(sent) != m_CurrentSend.bytes.size())
            return false;
        else
            m_SendOffset = m_CurrentSend.bytes.size();

        if (m_SendOffset == m_CurrentSend.bytes.size())
        {
            m_CurrentSend.bytes.clear();
            m_HasCurrentSend = false;
            m_SendOffset = 0;
        }
    }
    return true;
}

void NetworkClient::QueueReceived(const Net::PacketData& packet)
{
    EnterCriticalSection(&m_CSReceiveQueue);
    try
    {
        m_qReceiveQueue.push(packet);
    }
    catch (...)
    {
        LeaveCriticalSection(&m_CSReceiveQueue);
        throw;
    }
    LeaveCriticalSection(&m_CSReceiveQueue);
}

void NetworkClient::SetReceiveContext(Net::MatchContext context, Net::MembershipId membershipId)
{
    if (m_ReceiveContext.roomId != context.roomId || m_ReceiveContext.matchId != context.matchId
        || m_MembershipId != membershipId)
    {
        m_HasPlayerSequence = false;
        m_HasTargetSequence = false;
    }
    m_ReceiveContext = context;
    m_MembershipId = membershipId;
}

bool NetworkClient::ProcessTcpPacket(const Net::PacketData& packet)
{
    Net::TcpHeader header;
    if (Net::PacketCodec::ReadTcpHeader(packet.bytes.data(), packet.bytes.size(), header) != Net::DecodeStatus::Complete
        || header.size != packet.bytes.size() || (!m_HasConnectResult && header.type != Net::TcpType::ConnectResult))
        return false;
    const std::uint8_t* messageBytes = packet.bytes.data() + Net::TcpHeaderBytes;
    std::size_t size = packet.bytes.size() - Net::TcpHeaderBytes;
    switch (header.type)
    {
    case Net::TcpType::ConnectResult:
    {
        Net::ConnectResult result;
        if (m_HasConnectResult || !Net::PacketCodec::ReadMessage(header.type, messageBytes, size, result))
            return false;
        if (result.result != Net::ErrorCode::Ok)
        {
            QueueReceived(packet);
            return false;
        }
        if (!Net::IsValidSessionToken(result.sessionToken) || result.udpPort == 0)
            return false;
        m_SessionToken = result.sessionToken;
        m_ServerUdpAddr.sin_port = htons(result.udpPort);
        m_HasConnectResult = true;
        RegisterUDP();
        break;
    }
    case Net::TcpType::RequestRejected:
    {
        Net::RequestRejected result;
        if (!Net::PacketCodec::ReadMessage(header.type, messageBytes, size, result))
            return false;
        break;
    }
    case Net::TcpType::RoomEnterResult:
    {
        Net::RoomEnterResult result;
        std::uint32_t pending = m_PendingRoomRequestId;
        if (pending == 0 || header.requestId != pending
            || !Net::PacketCodec::ReadMessage(header.type, messageBytes, size, result))
            return false;
        m_PendingRoomRequestId = 0;
        if (result.result == Net::ErrorCode::Ok)
            SetReceiveContext(result.room.version.context, result.membershipId);
        break;
    }
    case Net::TcpType::LeaveRoomResult:
    {
        Net::LeaveRoomResult result;
        if (!Net::PacketCodec::ReadMessage(header.type, messageBytes, size, result))
            return false;
        if (result.result == Net::ErrorCode::Ok && result.roomId == m_ReceiveContext.roomId)
            SetReceiveContext({}, 0);
        break;
    }
    case Net::TcpType::RoomState:
    {
        Net::RoomState result;
        if (!Net::PacketCodec::ReadMessage(header.type, messageBytes, size, result))
            return false;
        break;
    }
    case Net::TcpType::ShotResolved:
    {
        Net::ShotResolved result;
        if (!Net::PacketCodec::ReadMessage(header.type, messageBytes, size, result))
            return false;
        if (result.version.context.roomId != m_ReceiveContext.roomId
            || result.version.context.matchId != m_ReceiveContext.matchId)
            return true;
        break;
    }
    case Net::TcpType::GameStarted:
    {
        Net::GameStarted result;
        if (!Net::PacketCodec::ReadMessage(header.type, messageBytes, size, result))
            return false;
        Net::MatchContext context = result.version.context;
        // 입장한 방의 같은 경기 또는 새 경기만 수신 문맥으로 삼는다.
        if (m_ReceiveContext.roomId == 0 || m_MembershipId == 0
            || context.roomId != m_ReceiveContext.roomId || context.matchId == 0
            || (m_ReceiveContext.matchId != 0 && context.matchId != m_ReceiveContext.matchId
                && !Net::IsNewerSequence(context.matchId, m_ReceiveContext.matchId)))
            return true;
        SetReceiveContext(context, m_MembershipId);
        break;
    }
    default:
        return false;
    }
    QueueReceived(packet);
    return true;
}

bool NetworkClient::ReceiveTcp()
{
    std::uint8_t bytes[4096];
    int received = recv(m_TCPSocket, reinterpret_cast<char*>(bytes), sizeof(bytes), 0);
    if (received == 0)
        return false;
    if (received == SOCKET_ERROR)
        return WSAGetLastError() == WSAEWOULDBLOCK;
    m_TcpReceiveBuffer.insert(m_TcpReceiveBuffer.end(), bytes, bytes + received);
    std::size_t offset = 0;
    while (offset < m_TcpReceiveBuffer.size() && !m_bStopRequested)
    {
        Net::TcpHeader header;
        Net::DecodeStatus status = Net::PacketCodec::ReadTcpHeader(
            m_TcpReceiveBuffer.data() + offset, m_TcpReceiveBuffer.size() - offset, header);
        if (status == Net::DecodeStatus::NeedMore)
            break;
        if (status == Net::DecodeStatus::Invalid)
            return false;
        Net::PacketData packet;
        packet.transport = Net::PacketTransport::Tcp;
        packet.bytes.assign(m_TcpReceiveBuffer.begin() + offset, m_TcpReceiveBuffer.begin() + offset + header.size);
        if (!ProcessTcpPacket(packet))
            return false;
        offset += header.size;
    }
    m_TcpReceiveBuffer.erase(m_TcpReceiveBuffer.begin(), m_TcpReceiveBuffer.begin() + offset);
    return true;
}

void NetworkClient::ReceiveUdp()
{
    std::uint8_t bytes[Net::MaxUdpPacketBytes];
    sockaddr_in source{};
    int addressSize = sizeof(source);
    int received = recvfrom(m_UDPSocket, reinterpret_cast<char*>(bytes), sizeof(bytes), 0,
        reinterpret_cast<sockaddr*>(&source), &addressSize);
    if (received <= 0 || source.sin_family != AF_INET
        || source.sin_addr.s_addr != m_ServerUdpAddr.sin_addr.s_addr
        || source.sin_port != m_ServerUdpAddr.sin_port || !m_HasConnectResult)
        return;
    Net::UdpHeader header;
    if (!Net::PacketCodec::ReadUdpHeader(bytes, static_cast<std::size_t>(received), header)
        || header.m_sessionToken != m_SessionToken)
        return;
    const std::uint8_t* messageBytes = bytes + Net::UdpHeaderBytes;
    std::size_t size = static_cast<std::size_t>(received) - Net::UdpHeaderBytes;
    if (header.type == Net::UdpType::RegisterResult)
    {
        Net::UdpRegisterResult result;
        if (!m_RegistrationSent || m_UDPRegistered || header.sequence != m_RegistrationSequence
            || header.context.roomId != 0 || header.context.matchId != 0 || header.membershipId != 0
            || header.serverTick != 0
            || !Net::PacketCodec::ReadMessage(header.type, messageBytes, size, result)
            || result.registrationSequence != m_RegistrationSequence)
            return;
        CompleteConnection(result);
        QueueReceived(Net::PacketData{Net::PacketTransport::Udp,
            std::vector<std::uint8_t>(bytes, bytes + received)});
        return;
    }
    if (!m_UDPRegistered || m_ReceiveContext.roomId == 0 || m_MembershipId == 0
        || header.context.roomId != m_ReceiveContext.roomId || header.context.matchId != m_ReceiveContext.matchId
        || header.membershipId != m_MembershipId)
        return;
    if (header.type == Net::UdpType::PlayerSnapshot)
    {
        Net::PlayerSnapshot snapshot;
        if (!Net::PacketCodec::ReadMessage(header.type, messageBytes, size, snapshot)
            || (m_HasPlayerSequence && !Net::IsNewerSequence(header.sequence, m_LastPlayerSequence)))
            return;
        m_LastPlayerSequence = header.sequence;
        m_HasPlayerSequence = true;
    }
    else if (header.type == Net::UdpType::TargetSnapshot)
    {
        Net::TargetSnapshot snapshot;
        if (!Net::PacketCodec::ReadMessage(header.type, messageBytes, size, snapshot)
            || (m_HasTargetSequence && !Net::IsNewerSequence(header.sequence, m_LastTargetSequence)))
            return;
        m_LastTargetSequence = header.sequence;
        m_HasTargetSequence = true;
    }
    else
        return;
    QueueReceived(Net::PacketData{Net::PacketTransport::Udp,
        std::vector<std::uint8_t>(bytes, bytes + received)});
}

void NetworkClient::NetworkLoop()
{
    if (GetCurrentThreadId() != m_NetworkThreadId)
        return;
    while (!m_bStopRequested)
    {
        if (!FlushSendQueue())
            break;
        fd_set reads;
        fd_set writes;
        fd_set errors;
        FD_ZERO(&reads);
        FD_ZERO(&writes);
        FD_ZERO(&errors);
        if (!m_TcpConnecting)
            FD_SET(m_TCPSocket, &reads);
        if (m_TcpConnecting)
        {
            FD_SET(m_TCPSocket, &writes);
            FD_SET(m_TCPSocket, &errors);
        }
        else if (m_HasCurrentSend && m_CurrentSend.transport == Net::PacketTransport::Tcp)
            FD_SET(m_TCPSocket, &writes);
        if (m_HasCurrentSend && m_CurrentSend.transport == Net::PacketTransport::Udp)
            FD_SET(m_UDPSocket, &writes);
        FD_SET(m_UDPSocket, &reads);
        // 기존 서버와 같은 짧은 대기로 종료 요청과 새 송신 큐를 확인한다.
        timeval wait{};
        wait.tv_usec = 10000;
        int ready = select(0, &reads, &writes, &errors, &wait);
        if (ready == SOCKET_ERROR || m_bStopRequested)
            break;
        if (m_TcpConnecting && (FD_ISSET(m_TCPSocket, &writes) || FD_ISSET(m_TCPSocket, &errors)))
        {
            int error = 0;
            int size = sizeof(error);
            if (getsockopt(m_TCPSocket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &size) == SOCKET_ERROR
                || error != 0)
                break;
            m_TcpConnecting = false;
        }
        if (FD_ISSET(m_TCPSocket, &reads) && !ReceiveTcp())
            break;
        if (FD_ISSET(m_UDPSocket, &reads))
            ReceiveUdp();
    }
}

void NetworkClient::UpdateNetwork()
{
    if (GetCurrentThreadId() != m_ControlThreadId)
        return;
    std::queue<Net::PacketData> received;
    EnterCriticalSection(&m_CSReceiveQueue);
    received.swap(m_qReceiveQueue);
    LeaveCriticalSection(&m_CSReceiveQueue);
    m_ReceivedPackets.clear();
    while (!received.empty())
    {
        Net::PacketData& packet = received.front();
        if (!m_RoomRequested && packet.transport == Net::PacketTransport::Udp)
        {
            Net::UdpPacket<Net::UdpRegisterResult> result;
            if (Net::PacketCodec::DecodeUdp(packet.bytes.data(), packet.bytes.size(), result)
                && result.packetData.result == Net::ErrorCode::Ok && IsConnected())
            {
                // 최초 UDP 등록이 승인된 뒤 기존 방 생성 요청을 한 번만 보낸다.
                m_RoomRequested = true;
                Net::PacketData request;
                std::uint32_t requestId = NextRequestId();
                // 송신 전에 번호를 공개해 빠른 입장 응답도 같은 요청인지 확인한다.
                m_PendingRoomRequestId = requestId;
                if (!Net::PacketCodec::EncodeTcp(Net::CreateRoomRequest{}, requestId, request)
                    || !SendPacket(request))
                {
                    m_PendingRoomRequestId = 0;
                    m_bStopRequested = true;
                }
            }
        }
        m_ReceivedPackets.push_back(std::move(packet));
        received.pop();
    }
}

Net::SessionToken NetworkClient::GetSessionToken() const
{
    // 등록 완료를 확인한 뒤에는 통신 스레드가 이 토큰을 다시 바꾸지 않는다.
    if (GetCurrentThreadId() != m_ControlThreadId || !IsConnected())
        return {};
    return m_SessionToken;
}

std::uint32_t NetworkClient::NextRequestId()
{
    if (GetCurrentThreadId() != m_ControlThreadId)
        return 0;
    return m_NextRequestId++;
}

void NetworkClient::DisconnectServer()
{
    m_bStopRequested = true;
    if (GetCurrentThreadId() != m_ControlThreadId)
        return;
    if (m_hNetworkThread != nullptr)
    {
        WaitForSingleObject(m_hNetworkThread, INFINITE);
        CloseHandle(m_hNetworkThread);
        m_hNetworkThread = nullptr;
    }
    if (m_TCPSocket != INVALID_SOCKET)
    {
        closesocket(m_TCPSocket);
        m_TCPSocket = INVALID_SOCKET;
    }
    if (m_UDPSocket != INVALID_SOCKET)
    {
        closesocket(m_UDPSocket);
        m_UDPSocket = INVALID_SOCKET;
    }
    EnterCriticalSection(&m_CSSendQueue);
    while (!m_qSendQueue.empty())
        m_qSendQueue.pop();
    LeaveCriticalSection(&m_CSSendQueue);
    EnterCriticalSection(&m_CSReceiveQueue);
    while (!m_qReceiveQueue.empty())
        m_qReceiveQueue.pop();
    LeaveCriticalSection(&m_CSReceiveQueue);
    m_ReceivedPackets.clear();
    m_RoomRequested = false;
    m_NextRequestId = 1;
    m_PendingRoomRequestId = 0;
    m_TcpReceiveBuffer.clear();
    m_CurrentSend.bytes.clear();
    m_HasCurrentSend = false;
    m_SendOffset = 0;
    m_SessionToken.fill(0);
    m_UDPRegistered = false;
    m_TcpConnecting = false;
    m_HasConnectResult = false;
    m_RegistrationSent = false;
    m_RegistrationSequence = 0;
    SetReceiveContext({}, 0);
    m_ServerTcpAddr = {};
    m_ServerUdpAddr = {};
    if (m_WinsockStarted)
    {
        WSACleanup();
        m_WinsockStarted = false;
    }
}
