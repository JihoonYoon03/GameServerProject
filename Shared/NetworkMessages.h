#pragma once
#include "NetworkPackets.h"
#include <variant>
#include <functional>

namespace Net
{
    enum class PacketTransport : std::uint8_t { Tcp, Udp };

    // TCP 또는 UDP 헤더까지 포함한 직렬화 결과. 소켓 송신에 사용할 바이트 목록이다.
    struct PacketData
    {
        PacketTransport transport = PacketTransport::Tcp;
        std::vector<std::uint8_t> bytes;
    };

    struct SendRequest
    {
        SessionToken recipientToken{};
        PacketData packet;
    };

    // 완성 패킷을 송신 큐에 전달한다. 큐의 잠금은 수신 측이 관리한다.
    using PacketSender = std::function<bool(const SendRequest&)>;

    struct RoomEnterCommand { bool UDPRegistered = false; };
    struct ClientDisconnected {};

    // 서버는 TCP 연결 또는 등록된 UDP 주소로 발신자를 확인한 뒤 토큰을 채운다.
    // 요청 본문에 적힌 토큰만 신뢰하지 않으며, 방 스레드는 명령 큐를 통해 요청을 처리한다.
    struct RoomCommand
    {
        SessionToken sessionToken{};
        std::uint32_t requestId = 0;
        MatchContext context;
        MembershipId membershipId = 0;
        ServerTimeMs receivedAtMs = 0;
        std::variant<RoomEnterCommand, LeaveRoomRequest, MapLoaded,
            ReadyChangeRequest, PlayerTransform, FireRequest, ClientDisconnected> body;
    };

    enum class RoomManagementEventType : std::uint8_t { SummaryChanged, RoomEmpty };

    struct RoomManagementEvent
    {
        RoomManagementEventType type = RoomManagementEventType::SummaryChanged;
        RoomInfo summary;
    };
}
