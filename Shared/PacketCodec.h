#pragma once
#include "NetworkMessages.h"

namespace Net
{
    // 본문 자료형에 대응하는 패킷 종류와 전송 방식을 정한다.
    template<class T> struct PacketTraits;

    template<> struct PacketTraits<ConnectResult>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::ConnectResult;
    };

    template<> struct PacketTraits<RequestRejected>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::RequestRejected;
    };

    template<> struct PacketTraits<RoomListRequest>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::RoomListRequest;
    };

    template<> struct PacketTraits<RoomListResult>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::RoomListResult;
    };

    template<> struct PacketTraits<CreateRoomRequest>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::CreateRoomRequest;
    };

    template<> struct PacketTraits<JoinRoomRequest>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::JoinRoomRequest;
    };

    template<> struct PacketTraits<RoomEnterResult>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::RoomEnterResult;
    };

    template<> struct PacketTraits<LeaveRoomRequest>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::LeaveRoomRequest;
    };

    template<> struct PacketTraits<LeaveRoomResult>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::LeaveRoomResult;
    };

    template<> struct PacketTraits<MapLoaded>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::MapLoaded;
    };

    template<> struct PacketTraits<ReadyChangeRequest>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::ReadyChangeRequest;
    };

    template<> struct PacketTraits<RoomState>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::RoomState;
    };

    template<> struct PacketTraits<GameStarted>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::GameStarted;
    };

    template<> struct PacketTraits<FireRequest>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::FireRequest;
    };

    template<> struct PacketTraits<ShotResolved>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::ShotResolved;
    };

    template<> struct PacketTraits<MatchTime>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::MatchTime;
    };

    template<> struct PacketTraits<GameResults>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::GameResults;
    };

    template<> struct PacketTraits<ReloadRequest>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::ReloadRequest;
    };

    template<> struct PacketTraits<WeaponState>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::WeaponState;
    };

    template<> struct PacketTraits<TimeProbeRequest>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::TimeProbeRequest;
    };

    template<> struct PacketTraits<TimeProbeResult>
    {
        static constexpr PacketTransport transport = PacketTransport::Tcp;
        static constexpr TcpType type = TcpType::TimeProbeResult;
    };

    template<> struct PacketTraits<UdpRegisterRequest>
    {
        static constexpr PacketTransport transport = PacketTransport::Udp;
        static constexpr UdpType type = UdpType::RegisterRequest;
    };

    template<> struct PacketTraits<UdpRegisterResult>
    {
        static constexpr PacketTransport transport = PacketTransport::Udp;
        static constexpr UdpType type = UdpType::RegisterResult;
    };

    template<> struct PacketTraits<PlayerTransform>
    {
        static constexpr PacketTransport transport = PacketTransport::Udp;
        static constexpr UdpType type = UdpType::PlayerTransform;
    };

    template<> struct PacketTraits<PlayerSnapshot>
    {
        static constexpr PacketTransport transport = PacketTransport::Udp;
        static constexpr UdpType type = UdpType::PlayerSnapshot;
    };

    template<> struct PacketTraits<TargetSnapshot>
    {
        static constexpr PacketTransport transport = PacketTransport::Udp;
        static constexpr UdpType type = UdpType::TargetSnapshot;
    };

    // 논리적인 헤더와 본문이다. 실제 송신에는 직렬화한 PacketData를 사용한다.
    template<class T> struct TcpPacket
    {
        TcpHeader header;
        T packetData;
    };

    template<class T> struct UdpPacket
    {
        UdpHeader header;
        T packetData;
    };

    enum class DecodeStatus { Complete, NeedMore, Invalid };

    // 정수와 단정밀도 실수의 비트는 네트워크 바이트 순서인 빅엔디언으로 기록한다.
    // 읽기에 실패하면 읽기 위치와 출력값을 바꾸지 않는다.
    void WriteU8(std::vector<std::uint8_t>& out, std::uint8_t value);
    void WriteU16(std::vector<std::uint8_t>& out, std::uint16_t value);
    void WriteU32(std::vector<std::uint8_t>& out, std::uint32_t value);
    void WriteU64(std::vector<std::uint8_t>& out, std::uint64_t value);
    void WriteF32(std::vector<std::uint8_t>& out, float value);
    bool ReadU8(const std::uint8_t* data, std::size_t size, std::size_t& offset, std::uint8_t& value);
    bool ReadU16(const std::uint8_t* data, std::size_t size, std::size_t& offset, std::uint16_t& value);
    bool ReadU32(const std::uint8_t* data, std::size_t size, std::size_t& offset, std::uint32_t& value);
    bool ReadU64(const std::uint8_t* data, std::size_t size, std::size_t& offset, std::uint64_t& value);
    bool ReadF32(const std::uint8_t* data, std::size_t size, std::size_t& offset, float& value);

    class PacketCodec
    {
    public:
        // 송신자는 헤더의 종류와 전체 길이를 본문과 맞춰서 채운다.
        // 종류나 길이가 맞지 않으면 거절하고 출력값을 바꾸지 않는다.
        template<class T>
        static bool EncodeTcp(const TcpPacket<T>& packet, PacketData& output)
        {
            if (packet.header.type != PacketTraits<T>::type)
                return false;

            std::vector<std::uint8_t> bytes;
            if (!EncodeBody(packet.packetData, bytes))
                return false;

            return EncodeTcpFrame(packet.header, bytes, output);
        }

        template<class T>
        static bool EncodeUdp(const UdpPacket<T>& packet, PacketData& output)
        {
            if (packet.header.type != PacketTraits<T>::type)
                return false;

            std::vector<std::uint8_t> bytes;
            if (!EncodeBody(packet.packetData, bytes))
                return false;

            return EncodeUdpFrame(packet.header, bytes, output);
        }

        // 누적 수신 버퍼에서 지정한 본문 자료형의 패킷 하나를 읽는다.
        // 수신 중이면 버퍼를 유지하고, 오류면 거절한다.
        template<class T>
        static DecodeStatus DecodeTcp(const std::uint8_t* data, std::size_t size,
            TcpPacket<T>& output, std::size_t& consumed)
        {
            consumed = 0;
            TcpHeader header;
            const DecodeStatus status = ReadTcpHeader(data, size, PacketTraits<T>::type, header);
            if (status != DecodeStatus::Complete)
                return status;

            T value{};
            if (!DecodeBody(header.type, data + TcpHeaderBytes,
                header.size - TcpHeaderBytes, value))
                return DecodeStatus::Invalid;

            output.header = header;
            output.packetData = value;
            consumed = header.size;
            return DecodeStatus::Complete;
        }

        // UDP는 데이터그램 하나 전체를 읽고 길이와 본문 자료형을 검사한다.
        template<class T>
        static bool DecodeUdp(const std::uint8_t* data, std::size_t size,
            UdpPacket<T>& output)
        {
            UdpHeader header;
            if (!ReadUdpHeader(data, size, PacketTraits<T>::type, header))
                return false;

            T value{};
            if (!DecodeBody(header.type, data + UdpHeaderBytes,
                header.size - UdpHeaderBytes, value))
                return false;

            output.header = header;
            output.packetData = value;
            return true;
        }

        static bool ValidateHeader(const TcpHeader& header);
        static bool ValidateHeader(const UdpHeader& header);

        // 본문 자료형마다 별도 함수로 읽고 검사한다.
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, ConnectResult& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, RequestRejected& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, RoomListRequest& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, RoomListResult& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, CreateRoomRequest& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, JoinRoomRequest& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, RoomEnterResult& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, LeaveRoomRequest& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, LeaveRoomResult& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, MapLoaded& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, ReadyChangeRequest& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, RoomState& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, GameStarted& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, FireRequest& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, ShotResolved& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, MatchTime& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, GameResults& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, ReloadRequest& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, WeaponState& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, TimeProbeRequest& output);
        static bool DecodeBody(TcpType type, const std::uint8_t* data, std::size_t size, TimeProbeResult& output);
        static bool DecodeBody(UdpType type, const std::uint8_t* data, std::size_t size, UdpRegisterRequest& output);
        static bool DecodeBody(UdpType type, const std::uint8_t* data, std::size_t size, UdpRegisterResult& output);
        static bool DecodeBody(UdpType type, const std::uint8_t* data, std::size_t size, PlayerTransform& output);
        static bool DecodeBody(UdpType type, const std::uint8_t* data, std::size_t size, PlayerSnapshot& output);
        static bool DecodeBody(UdpType type, const std::uint8_t* data, std::size_t size, TargetSnapshot& output);

        static bool ValidateBody(const ConnectResult& body);
        static bool ValidateBody(const RequestRejected& body);
        static bool ValidateBody(const RoomListRequest& body);
        static bool ValidateBody(const RoomListResult& body);
        static bool ValidateBody(const CreateRoomRequest& body);
        static bool ValidateBody(const JoinRoomRequest& body);
        static bool ValidateBody(const RoomEnterResult& body);
        static bool ValidateBody(const LeaveRoomRequest& body);
        static bool ValidateBody(const LeaveRoomResult& body);
        static bool ValidateBody(const MapLoaded& body);
        static bool ValidateBody(const ReadyChangeRequest& body);
        static bool ValidateBody(const RoomState& body);
        static bool ValidateBody(const GameStarted& body);
        static bool ValidateBody(const FireRequest& body);
        static bool ValidateBody(const ShotResolved& body);
        static bool ValidateBody(const MatchTime& body);
        static bool ValidateBody(const GameResults& body);
        static bool ValidateBody(const ReloadRequest& body);
        static bool ValidateBody(const WeaponState& body);
        static bool ValidateBody(const TimeProbeRequest& body);
        static bool ValidateBody(const TimeProbeResult& body);
        static bool ValidateBody(const UdpRegisterRequest& body);
        static bool ValidateBody(const UdpRegisterResult& body);
        static bool ValidateBody(const PlayerTransform& body);
        static bool ValidateBody(const PlayerSnapshot& body);
        static bool ValidateBody(const TargetSnapshot& body);

    private:
        static bool EncodeBody(const ConnectResult& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const RequestRejected& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const RoomListRequest& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const RoomListResult& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const CreateRoomRequest& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const JoinRoomRequest& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const RoomEnterResult& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const LeaveRoomRequest& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const LeaveRoomResult& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const MapLoaded& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const ReadyChangeRequest& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const RoomState& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const GameStarted& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const FireRequest& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const ShotResolved& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const MatchTime& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const GameResults& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const ReloadRequest& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const WeaponState& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const TimeProbeRequest& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const TimeProbeResult& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const UdpRegisterRequest& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const UdpRegisterResult& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const PlayerTransform& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const PlayerSnapshot& body, std::vector<std::uint8_t>& bytes);
        static bool EncodeBody(const TargetSnapshot& body, std::vector<std::uint8_t>& bytes);

        static bool EncodeTcpFrame(const TcpHeader& header,
            const std::vector<std::uint8_t>& bytes, PacketData& output);
        static bool EncodeUdpFrame(const UdpHeader& header,
            const std::vector<std::uint8_t>& bytes, PacketData& output);
        static DecodeStatus ReadTcpHeader(const std::uint8_t* data, std::size_t size,
            TcpType expected, TcpHeader& output);
        static bool ReadUdpHeader(const std::uint8_t* data, std::size_t size,
            UdpType expected, UdpHeader& output);
    };
}
