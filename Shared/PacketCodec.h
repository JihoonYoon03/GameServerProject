#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include "NetworkMessages.h"
#include "NetworkConversions.h"
#include <cstring>
#include <cmath>
#include <utility>

namespace Net
{
    static_assert(sizeof(float) == 4, "Packet floats must be 4 bytes");

    template<class T>
    struct UdpPacket
    {
        UdpHeader header;
        T packetData;
    };

    enum class DecodeStatus { Complete, NeedMore, Invalid };

    namespace PacketDetail
    {
        inline bool Valid(ErrorCode value)
        {
            return value <= ErrorCode::ServerFull && static_cast<std::uint16_t>(value) != 2;
        }
        inline bool Valid(GameState value) { return value <= GameState::Results; }
        inline bool Valid(ReadyState value) { return value <= ReadyState::Playing; }

        // Convert one scalar in place. Float values use their unchanged 32-bit representation.
        inline bool ConvertByteOrder(void* value, std::size_t byteCount, bool reading)
        {
            switch (byteCount)
            {
            case 1:
                return true;
            case 2:
            {
                std::uint16_t number;
                std::memcpy(&number, value, 2);
                number = reading ? ntohs(number) : htons(number);
                std::memcpy(value, &number, 2);
                return true;
            }
            case 4:
            {
                std::uint32_t number;
                std::memcpy(&number, value, 4);
                number = reading ? ntohl(number) : htonl(number);
                std::memcpy(value, &number, 4);
                return true;
            }
            case 8:
            {
                std::uint64_t number;
                std::memcpy(&number, value, 8);
                number = reading ? ntohll(number) : htonll(number);
                std::memcpy(value, &number, 8);
                return true;
            }
            default:
                return false;
            }
        }

        struct PacketStream
        {
            bool reading;
            std::vector<std::uint8_t>* output;
            const std::uint8_t* input;
            std::size_t size;
            std::size_t offset = 0;

            explicit PacketStream(std::vector<std::uint8_t>& bytes)
                : reading(false), output(&bytes), input(nullptr), size(0) {}
            PacketStream(const std::uint8_t* data, std::size_t length)
                : reading(true), output(nullptr), input(data), size(length) {}

            bool Value(void* value, std::size_t byteCount)
            {
                if (byteCount != 1 && byteCount != 2 && byteCount != 4 && byteCount != 8)
                    return false;
                std::uint8_t bytes[8];
                if (reading)
                {
                    if (input == nullptr || offset > size || byteCount > size - offset)
                        return false;
                    std::memcpy(bytes, input + offset, byteCount);
                }
                else
                    std::memcpy(bytes, value, byteCount);

                if (!ConvertByteOrder(bytes, byteCount, reading))
                    return false;
                if (reading)
                {
                    std::memcpy(value, bytes, byteCount);
                    offset += byteCount;
                }
                else
                {
                    std::size_t previousSize = output->size();
                    output->resize(previousSize + byteCount);
                    std::memcpy(output->data() + previousSize, bytes, byteCount);
                }
                return true;
            }
        };

        inline bool Serialize(PacketStream& io, std::uint8_t& value)
        {
            return io.Value(&value, sizeof(value));
        }
        inline bool Serialize(PacketStream& io, std::uint16_t& value)
        {
            return io.Value(&value, sizeof(value));
        }
        inline bool Serialize(PacketStream& io, std::uint32_t& value)
        {
            return io.Value(&value, sizeof(value));
        }
        inline bool Serialize(PacketStream& io, std::uint64_t& value)
        {
            return io.Value(&value, sizeof(value));
        }
        inline bool Serialize(PacketStream& io, float& value)
        {
            return io.Value(&value, sizeof(value));
        }

        inline bool Serialize(PacketStream& io, TcpType& value)
        {
            std::uint16_t number = static_cast<std::uint16_t>(value);
            if (!Serialize(io, number))
                return false;
            value = static_cast<TcpType>(number);
            return true;
        }
        inline bool Serialize(PacketStream& io, UdpType& value)
        {
            std::uint16_t number = static_cast<std::uint16_t>(value);
            if (!Serialize(io, number))
                return false;
            value = static_cast<UdpType>(number);
            return true;
        }
        inline bool Serialize(PacketStream& io, ErrorCode& value)
        {
            std::uint16_t number = static_cast<std::uint16_t>(value);
            if (!Serialize(io, number))
                return false;
            value = static_cast<ErrorCode>(number);
            return true;
        }
        inline bool Serialize(PacketStream& io, GameState& value)
        {
            std::uint8_t number = static_cast<std::uint8_t>(value);
            if (!Serialize(io, number))
                return false;
            value = static_cast<GameState>(number);
            return true;
        }
        inline bool Serialize(PacketStream& io, ReadyState& value)
        {
            std::uint8_t number = static_cast<std::uint8_t>(value);
            if (!Serialize(io, number))
                return false;
            value = static_cast<ReadyState>(number);
            return true;
        }

        inline bool Serialize(PacketStream& io, SessionToken& value)
        {
            for (std::size_t i = 0; i < SessionTokenBytes; ++i)
                if (!Serialize(io, value[i]))
                    return false;
            return true;
        }
        // 행렬 원소는 기존과 같이 열을 먼저, 행을 나중에 순회한다.
        inline bool Serialize(PacketStream& io, glm::mat4& value)
        {
            for (int col = 0; col < 4; ++col)
                for (int row = 0; row < 4; ++row)
                    if (!Serialize(io, value[col][row]))
                        return false;
            return true;
        }
        inline bool Serialize(PacketStream& io, glm::vec3& value)
        {
            return Serialize(io, value.x) && Serialize(io, value.y) && Serialize(io, value.z);
        }
        inline bool Serialize(PacketStream& io, PlayerPose& value)
        {
            return Serialize(io, value.position) && Serialize(io, value.yaw) && Serialize(io, value.pitch)
                && IsFinitePlayerPose(value);
        }
        inline bool Serialize(PacketStream& io, BoxData& value)
        {
            return Serialize(io, value.min)
                && Serialize(io, value.max);
        }
        inline bool Serialize(PacketStream& io, MatchContext& value)
        {
            return Serialize(io, value.roomId)
                && Serialize(io, value.matchId);
        }
        inline bool Serialize(PacketStream& io, RoomVersion& value)
        {
            return Serialize(io, value.context)
                && Serialize(io, value.revision);
        }
        inline bool Serialize(PacketStream& io, AreaData& value)
        {
            return Serialize(io, value.areaId)
                && Serialize(io, value.movementBounds)
                && Serialize(io, value.spawnTransform);
        }
        inline bool Serialize(PacketStream& io, MatchRules& value)
        {
            return Serialize(io, value.durationMs)
                && Serialize(io, value.activeTargetCount)
                && Serialize(io, value.pointsPerHit) && value.activeTargetCount <= MaxTargets;
        }
        inline bool Serialize(PacketStream& io, PlayerStats& value)
        {
            return Serialize(io, value.score)
                && Serialize(io, value.shotsFired);
        }
        inline bool Serialize(PacketStream& io, PlayerResult& value)
        {
            return Serialize(io, value.sessionToken) && Serialize(io, value.stats)
                && Serialize(io, value.rank) && value.rank <= MaxPlayers;
        }
        inline bool Serialize(PacketStream& io, PlayerData& value)
        {
            return Serialize(io, value.m_sessionToken)
                && Serialize(io, value.pose)
                && Serialize(io, value.areaId)
                && Serialize(io, value.ready)
                && Serialize(io, value.mapLoaded)
                && Serialize(io, value.stats) && Valid(value.ready) && value.mapLoaded <= 1;
        }

        // 목록은 개수를 먼저 검사하고 유효한 원소만 처리한다.
        template<class T>
        inline bool SerializeList(PacketStream& io, std::vector<T>& values, std::size_t limit)
        {
            if (!io.reading && values.size() > limit)
                return false;
            std::uint16_t count = io.reading ? 0 : static_cast<std::uint16_t>(values.size());
            if (!Serialize(io, count) || count > limit)
                return false;
            if (io.reading)
                values.resize(count);
            for (std::size_t i = 0; i < count; ++i)
                if (!Serialize(io, values[i]))
                    return false;
            return true;
        }
        inline bool Serialize(PacketStream& io, RoomStateData& value)
        {
            return Serialize(io, value.version) && Serialize(io, value.state)
                && Serialize(io, value.capacity) && Serialize(io, value.rules)
                && SerializeList(io, value.vAreas, MaxPlayers)
                && SerializeList(io, value.vPlayers, MaxPlayers)
                && value.capacity <= MaxPlayers && Valid(value.state);
        }
        inline bool Serialize(PacketStream& io, PlayerTransformEntry& value)
        {
            return Serialize(io, value.m_sessionToken)
                && Serialize(io, value.pose)
                && Serialize(io, value.lastProcessedInput);
        }
        inline bool Serialize(PacketStream& io, TargetMotionData& value)
        {
            return Serialize(io, value.position)
                && Serialize(io, value.velocity);
        }
        inline bool Serialize(PacketStream& io, TcpHeader& value)
        {
            return Serialize(io, value.size)
                && Serialize(io, value.type)
                && Serialize(io, value.requestId);
        }
        inline bool Serialize(PacketStream& io, UdpHeader& value)
        {
            return Serialize(io, value.magic)
                && Serialize(io, value.type)
                && Serialize(io, value.m_sessionToken)
                && Serialize(io, value.sequence)
                && Serialize(io, value.context)
                && Serialize(io, value.membershipId)
                && Serialize(io, value.serverTick);
        }
        inline bool Serialize(PacketStream& io, ConnectResult& value)
        {
            return Serialize(io, value.result)
                && Serialize(io, value.sessionToken)
                && Serialize(io, value.udpPort)
                && Serialize(io, value.serverTimeMs)
                && Serialize(io, value.tickRate) && Valid(value.result);
        }
        inline bool Serialize(PacketStream& io, RequestRejected& value)
        {
            return Serialize(io, value.reason)
                && Serialize(io, value.context) && Valid(value.reason);
        }
        inline bool Serialize(PacketStream&, CreateRoomRequest&)
        {
            return true;
        }
        inline bool Serialize(PacketStream& io, RoomEnterResult& value)
        {
            if (!Serialize(io, value.result) || !Valid(value.result))
                return false;
            return value.result != ErrorCode::Ok
                || (Serialize(io, value.membershipId) && Serialize(io, value.room));
        }
        inline bool Serialize(PacketStream& io, LeaveRoomRequest& value)
        {
            return Serialize(io, value.roomId);
        }
        inline bool Serialize(PacketStream& io, LeaveRoomResult& value)
        {
            return Serialize(io, value.result)
                && Serialize(io, value.roomId) && Valid(value.result);
        }
        inline bool Serialize(PacketStream& io, MapLoaded& value)
        {
            return Serialize(io, value.roomId);
        }
        inline bool Serialize(PacketStream& io, StartGameRequest& value)
        {
            return Serialize(io, value.roomId) && value.roomId != 0;
        }
        inline bool Serialize(PacketStream& io, RoomState& value)
        {
            return Serialize(io, value.room);
        }
        inline bool Serialize(PacketStream& io, GameStarted& value)
        {
            return Serialize(io, value.version) && Serialize(io, value.serverTimeMs)
                && Serialize(io, value.startsAtMs) && Serialize(io, value.endsAtMs)
                && Serialize(io, value.rules) && SerializeList(io, value.players, MaxPlayers);
        }
        inline bool Serialize(PacketStream& io, MatchTime& value)
        {
            return Serialize(io, value.context) && Serialize(io, value.serverTimeMs)
                && Serialize(io, value.endsAtMs) && value.serverTimeMs <= value.endsAtMs;
        }
        inline bool Serialize(PacketStream& io, MatchEndReason& value)
        {
            std::uint8_t number = static_cast<std::uint8_t>(value);
            if (!Serialize(io, number) || number > static_cast<std::uint8_t>(MatchEndReason::ServerStopped))
                return false;
            value = static_cast<MatchEndReason>(number);
            return true;
        }
        inline bool Serialize(PacketStream& io, GameResults& value)
        {
            return Serialize(io, value.room) && value.room.state == GameState::Results
                && Serialize(io, value.endedAtMs) && Serialize(io, value.reason)
                && SerializeList(io, value.players, MaxPlayers);
        }
        inline bool Serialize(PacketStream& io, ShotOutcome& value)
        {
            std::uint8_t number = static_cast<std::uint8_t>(value);
            if (!Serialize(io, number) || number > static_cast<std::uint8_t>(ShotOutcome::BlockedByWorld))
                return false;
            value = static_cast<ShotOutcome>(number);
            return true;
        }
        inline bool Serialize(PacketStream& io, FireRequest& value)
        {
            if (!Serialize(io, value.context) || !Serialize(io, value.membershipId)
                || !Serialize(io, value.shotId) || !Serialize(io, value.inputSequence)
                || !Serialize(io, value.shooterTransform) || !Serialize(io, value.origin))
                return false;
            for (int col = 0; col < 4; ++col)
                for (int row = 0; row < 4; ++row)
                    if (!std::isfinite(value.shooterTransform[col][row]))
                        return false;
            return std::isfinite(value.origin.x) && std::isfinite(value.origin.y) && std::isfinite(value.origin.z);
        }
        inline bool Serialize(PacketStream& io, ShotResolved& value)
        {
            return Serialize(io, value.version) && Serialize(io, value.shooterToken)
                && Serialize(io, value.shotId) && Serialize(io, value.outcome)
                && Serialize(io, value.shooterStats);
        }
        inline bool Serialize(PacketStream&, UdpRegisterRequest&)
        {
            return true;
        }
        inline bool Serialize(PacketStream& io, UdpRegisterResult& value)
        {
            return Serialize(io, value.result)
                && Serialize(io, value.registrationSequence) && Valid(value.result);
        }
        inline bool Serialize(PacketStream& io, PlayerTransform& value)
        {
            return Serialize(io, value.inputSequence)
                && Serialize(io, value.pose);
        }
        template<class T, std::size_t Count>
        inline bool SerializeArray(PacketStream& io, std::array<T, Count>& values, std::size_t count)
        {
            if (count > values.size())
                return false;
            for (std::size_t i = 0; i < count; ++i)
                if (!Serialize(io, values[i]))
                    return false;
            return true;
        }
        inline bool Serialize(PacketStream& io, PlayerSnapshot& value)
        {
            return Serialize(io, value.serverTimeMs) && Serialize(io, value.requiredRevision)
                && Serialize(io, value.playerCount) && SerializeArray(io, value.players, value.playerCount);
        }
        inline bool Serialize(PacketStream& io, TargetSnapshot& value)
        {
            return Serialize(io, value.targetCount) && SerializeArray(io, value.targets, value.targetCount);
        }

    }

    class PacketCodec
    {
    public:

        template<class T>
        static bool EncodeTcp(const T& message, std::uint32_t requestId, PacketData& output)
        {
            std::vector<std::uint8_t> bytes;
            if (!WriteMessage(message, bytes) || bytes.size() > MaxTcpPacketBytes - TcpHeaderBytes)
                return false;

            TcpHeader header;
            header.type = T::type;
            header.requestId = requestId;
            header.size = static_cast<std::uint32_t>(TcpHeaderBytes + bytes.size());
            return EncodeTcpFrame(header, bytes, output);
        }

        template<class T>
        static bool EncodeUdp(const UdpPacket<T>& packet, PacketData& output)
        {
            if (packet.header.type != T::type)
                return false;

            std::vector<std::uint8_t> bytes;
            if (!WriteMessage(packet.packetData, bytes))
                return false;
            return EncodeUdpFrame(packet.header, bytes, output);
        }

        template<class T>
        static bool DecodeUdp(const std::uint8_t* data, std::size_t size, UdpPacket<T>& output)
        {
            UdpHeader header;
            if (!ReadUdpHeader(data, size, header) || header.type != T::type)
                return false;

            T message{};
            if (!ReadMessage(header.type, data + UdpHeaderBytes, size - UdpHeaderBytes, message))
                return false;

            output.header = header;
            output.packetData = std::move(message);
            return true;
        }

        // Apply fields only after the kind, values and exact message length are valid.
        template<class Type, class T>
        static bool ReadMessage(Type type, const std::uint8_t* data, std::size_t size, T& output)
        {
            std::size_t limit = MessageLimit(type);
            if (type != T::type || size > limit || (data == nullptr && size != 0))
                return false;

            T message{};
            PacketDetail::PacketStream stream(data, size);
            if (!PacketDetail::Serialize(stream, message) || stream.offset != size)
                return false;
            output = std::move(message);
            return true;
        }

        static DecodeStatus ReadTcpHeader(const std::uint8_t* data, std::size_t size, TcpHeader& output);
        static bool ReadUdpHeader(const std::uint8_t* data, std::size_t size, UdpHeader& output);
        static bool ValidateHeader(const TcpHeader& header);
        static bool ValidateHeader(const UdpHeader& header);

    private:
        static std::size_t MessageLimit(TcpType) { return MaxTcpPacketBytes - TcpHeaderBytes; }
        static std::size_t MessageLimit(UdpType) { return MaxUdpPacketBytes - UdpHeaderBytes; }

        template<class T>
        static bool WriteMessage(const T& message, std::vector<std::uint8_t>& bytes)
        {
            T fields = message;
            PacketDetail::PacketStream stream(bytes);
            return PacketDetail::Serialize(stream, fields);
        }

        static bool EncodeTcpFrame(const TcpHeader& header,
            const std::vector<std::uint8_t>& bytes, PacketData& output);
        static bool EncodeUdpFrame(const UdpHeader& header,
            const std::vector<std::uint8_t>& bytes, PacketData& output);
    };
}
