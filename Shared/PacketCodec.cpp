#include "PacketCodec.h"
#include "NetworkConversions.h"
#include <winsock2.h>
#include <cstring>
#include <limits>

#pragma comment(lib, "Ws2_32.lib")

namespace Net
{
    static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

    namespace
    {
        template<class T>
        void WriteNetworkValue(std::vector<std::uint8_t>& bytes, T value)
        {
            const std::uint8_t* data = reinterpret_cast<const std::uint8_t*>(&value);
            bytes.insert(bytes.end(), data, data + sizeof(value));
        }

        template<class T>
        bool ReadNetworkValue(const std::uint8_t* data, std::size_t size, std::size_t& offset, T& value)
        {
            if (data == nullptr || offset > size || sizeof(value) > size - offset)
                return false;

            std::memcpy(&value, data + offset, sizeof(value));
            offset += sizeof(value);
            return true;
        }

        bool CanReadBody(const std::uint8_t* data, std::size_t size, std::size_t limit)
        {
            return size <= limit && (data != nullptr || size == 0);
        }
    }

    void WriteU8(std::vector<std::uint8_t>& bytes, std::uint8_t value)
    {
        bytes.push_back(value);
    }
    void WriteU16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
    {
        WriteNetworkValue(bytes, htons(value));
    }
    void WriteU32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
    {
        WriteNetworkValue(bytes, htonl(value));
    }
    void WriteU64(std::vector<std::uint8_t>& bytes, std::uint64_t value)
    {
        WriteNetworkValue(bytes, htonll(value));
    }
    void WriteF32(std::vector<std::uint8_t>& bytes, float value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        WriteU32(bytes, bits);
    }

    bool ReadU8(const std::uint8_t* data, std::size_t size,
        std::size_t& offset, std::uint8_t& value)
    {
        if (data == nullptr || offset >= size)
            return false;

        value = data[offset];
        ++offset;
        return true;
    }
    bool ReadU16(const std::uint8_t* data, std::size_t size,
        std::size_t& offset, std::uint16_t& value)
    {
        std::uint16_t networkValue = 0;
        if (!ReadNetworkValue(data, size, offset, networkValue))
            return false;

        value = ntohs(networkValue);
        return true;
    }
    bool ReadU32(const std::uint8_t* data, std::size_t size,
        std::size_t& offset, std::uint32_t& value)
    {
        std::uint32_t networkValue = 0;
        if (!ReadNetworkValue(data, size, offset, networkValue))
            return false;

        value = ntohl(networkValue);
        return true;
    }
    bool ReadU64(const std::uint8_t* data, std::size_t size,
        std::size_t& offset, std::uint64_t& value)
    {
        std::uint64_t networkValue = 0;
        if (!ReadNetworkValue(data, size, offset, networkValue))
            return false;

        value = ntohll(networkValue);
        return true;
    }
    bool ReadF32(const std::uint8_t* data, std::size_t size,
        std::size_t& offset, float& value)
    {
        std::uint32_t bits = 0;
        if (!ReadU32(data, size, offset, bits))
        {
            return false;
        }
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }

    namespace
    {
        // 읽기와 쓰기가 같은 필드 순서를 사용하도록 작업 위치를 모은다.
        struct PacketIO
        {
            bool reading;
            std::vector<std::uint8_t>* output;
            const std::uint8_t* input;
            std::size_t size;
            std::size_t offset = 0;

            explicit PacketIO(std::vector<std::uint8_t>& bytes)
                : reading(false), output(&bytes), input(nullptr), size(0) {}
            PacketIO(const std::uint8_t* data, std::size_t length)
                : reading(true), output(nullptr), input(data), size(length) {}
        };

        bool Transfer(PacketIO& io, std::uint8_t& value)
        {
            if (io.reading)
                return ReadU8(io.input, io.size, io.offset, value);
            WriteU8(*io.output, value);
            return true;
        }
        bool Transfer(PacketIO& io, std::uint16_t& value)
        {
            if (io.reading)
                return ReadU16(io.input, io.size, io.offset, value);
            WriteU16(*io.output, value);
            return true;
        }
        bool Transfer(PacketIO& io, std::uint32_t& value)
        {
            if (io.reading)
                return ReadU32(io.input, io.size, io.offset, value);
            WriteU32(*io.output, value);
            return true;
        }
        bool Transfer(PacketIO& io, std::uint64_t& value)
        {
            if (io.reading)
                return ReadU64(io.input, io.size, io.offset, value);
            WriteU64(*io.output, value);
            return true;
        }
        bool Transfer(PacketIO& io, float& value)
        {
            if (io.reading)
                return ReadF32(io.input, io.size, io.offset, value);
            WriteF32(*io.output, value);
            return true;
        }

        // 열거형마다 명세에 정해진 정수 폭을 사용한다.
        template<class T, class Integer>
        bool TransferEnum(PacketIO& io, T& value, Integer raw)
        {
            if (!Transfer(io, raw))
                return false;
            value = static_cast<T>(raw);
            return true;
        }
        bool Transfer(PacketIO& io, ErrorCode& value)
        {
            return TransferEnum(io, value, static_cast<std::uint16_t>(value));
        }
        bool Transfer(PacketIO& io, GameState& value)
        {
            return TransferEnum(io, value, static_cast<std::uint8_t>(value));
        }
        bool Transfer(PacketIO& io, ReadyState& value)
        {
            return TransferEnum(io, value, static_cast<std::uint8_t>(value));
        }
        bool Transfer(PacketIO& io, ShotOutcome& value)
        {
            return TransferEnum(io, value, static_cast<std::uint8_t>(value));
        }
        bool Transfer(PacketIO& io, MatchEndReason& value)
        {
            return TransferEnum(io, value, static_cast<std::uint8_t>(value));
        }
        bool Transfer(PacketIO& io, TcpType& value)
        {
            return TransferEnum(io, value, static_cast<std::uint16_t>(value));
        }
        bool Transfer(PacketIO& io, UdpType& value)
        {
            return TransferEnum(io, value, static_cast<std::uint16_t>(value));
        }

        bool Transfer(PacketIO& io, SessionToken& value)
        {
            for (std::size_t i = 0; i < SessionTokenBytes; ++i)
                if (!Transfer(io, value[i]))
                    return false;
            return true;
        }
        // 행렬 원소는 기존과 같이 열을 먼저, 행을 나중에 순회한다.
        bool Transfer(PacketIO& io, glm::mat4& value)
        {
            for (int col = 0; col < 4; ++col)
                for (int row = 0; row < 4; ++row)
                    if (!Transfer(io, value[col][row]))
                        return false;
            return true;
        }
        bool Transfer(PacketIO& io, glm::vec3& value)
        {
            return Transfer(io, value.x) && Transfer(io, value.y) && Transfer(io, value.z);
        }
        bool Transfer(PacketIO& io, PlayerPose& value)
        {
            return Transfer(io, value.position) && Transfer(io, value.yaw) && Transfer(io, value.pitch)
                && (!io.reading || IsFinitePlayerPose(value));
        }
        bool Transfer(PacketIO& io, BoxData& value)
        {
            return Transfer(io, value.min)
                && Transfer(io, value.max);
        }
        bool Transfer(PacketIO& io, MatchContext& value)
        {
            return Transfer(io, value.roomId)
                && Transfer(io, value.matchId);
        }
        bool Transfer(PacketIO& io, RoomVersion& value)
        {
            return Transfer(io, value.context)
                && Transfer(io, value.revision);
        }
        bool Transfer(PacketIO& io, AreaData& value)
        {
            return Transfer(io, value.areaId)
                && Transfer(io, value.movementBounds)
                && Transfer(io, value.spawnTransform);
        }
        bool Transfer(PacketIO& io, MatchRules& value)
        {
            return Transfer(io, value.durationMs)
                && Transfer(io, value.activeTargetCount)
                && Transfer(io, value.pointsPerHit);
        }
        bool Transfer(PacketIO& io, PlayerStats& value)
        {
            return Transfer(io, value.score)
                && Transfer(io, value.shotsFired);
        }
        bool Transfer(PacketIO& io, PlayerData& value)
        {
            return Transfer(io, value.m_sessionToken)
                && Transfer(io, value.pose)
                && Transfer(io, value.areaId)
                && Transfer(io, value.ready)
                && Transfer(io, value.mapLoaded)
                && Transfer(io, value.stats);
        }
        bool Transfer(PacketIO& io, RoomInfo& value)
        {
            return Transfer(io, value.roomId)
                && Transfer(io, value.playerCount)
                && Transfer(io, value.capacity)
                && Transfer(io, value.state);
        }
        bool Transfer(PacketIO& io, PlayerResult& value)
        {
            return Transfer(io, value.m_sessionToken)
                && Transfer(io, value.stats)
                && Transfer(io, value.rank);
        }

        // 목록은 개수를 먼저 검사하고 유효한 원소만 처리한다.
        template<class T>
        bool TransferList(PacketIO& io, std::vector<T>& values, std::size_t limit)
        {
            if (!io.reading && values.size() > limit)
                return false;
            std::uint16_t count = io.reading ? 0 : static_cast<std::uint16_t>(values.size());
            if (!Transfer(io, count) || count > limit)
                return false;
            if (io.reading)
                values.resize(count);
            for (std::size_t i = 0; i < count; ++i)
                if (!Transfer(io, values[i]))
                    return false;
            return true;
        }
        bool Transfer(PacketIO& io, RoomStateData& value)
        {
            return Transfer(io, value.version) && Transfer(io, value.state)
                && Transfer(io, value.capacity) && Transfer(io, value.rules)
                && TransferList(io, value.vAreas, MaxPlayers)
                && TransferList(io, value.vPlayers, MaxPlayers);
        }
        bool Transfer(PacketIO& io, PlayerTransformEntry& value)
        {
            return Transfer(io, value.m_sessionToken)
                && Transfer(io, value.pose)
                && Transfer(io, value.lastProcessedInput);
        }
        bool Transfer(PacketIO& io, TargetMotionData& value)
        {
            return Transfer(io, value.position)
                && Transfer(io, value.velocity);
        }
        bool Transfer(PacketIO& io, TcpHeader& value)
        {
            return Transfer(io, value.size)
                && Transfer(io, value.type)
                && Transfer(io, value.requestId);
        }
        bool Transfer(PacketIO& io, UdpHeader& value)
        {
            return Transfer(io, value.magic)
                && Transfer(io, value.type)
                && Transfer(io, value.m_sessionToken)
                && Transfer(io, value.sequence)
                && Transfer(io, value.context)
                && Transfer(io, value.membershipId)
                && Transfer(io, value.serverTick);
        }
        bool Transfer(PacketIO& io, ConnectResult& value)
        {
            return Transfer(io, value.result)
                && Transfer(io, value.sessionToken)
                && Transfer(io, value.udpPort)
                && Transfer(io, value.serverTimeMs)
                && Transfer(io, value.tickRate);
        }
        bool Transfer(PacketIO& io, RequestRejected& value)
        {
            return Transfer(io, value.reason)
                && Transfer(io, value.context);
        }
        bool Transfer(PacketIO&, RoomListRequest&)
        {
            return true;
        }
        bool Transfer(PacketIO& io, RoomListResult& value)
        {
            return TransferList(io, value.rooms, MaxRooms);
        }
        bool Transfer(PacketIO&, CreateRoomRequest&)
        {
            return true;
        }
        bool Transfer(PacketIO& io, JoinRoomRequest& value)
        {
            return Transfer(io, value.roomId);
        }
        bool Transfer(PacketIO& io, RoomEnterResult& value)
        {
            if (!Transfer(io, value.result))
                return false;
            return value.result != ErrorCode::Ok
                || (Transfer(io, value.membershipId) && Transfer(io, value.room));
        }
        bool Transfer(PacketIO& io, LeaveRoomRequest& value)
        {
            return Transfer(io, value.roomId);
        }
        bool Transfer(PacketIO& io, LeaveRoomResult& value)
        {
            return Transfer(io, value.result)
                && Transfer(io, value.roomId);
        }
        bool Transfer(PacketIO& io, MapLoaded& value)
        {
            return Transfer(io, value.roomId);
        }
        bool Transfer(PacketIO& io, ReadyChangeRequest& value)
        {
            return Transfer(io, value.roomId)
                && Transfer(io, value.ready);
        }
        bool Transfer(PacketIO& io, RoomState& value)
        {
            return Transfer(io, value.room);
        }
        bool Transfer(PacketIO& io, GameStarted& value)
        {
            return Transfer(io, value.version) && Transfer(io, value.serverTimeMs)
                && Transfer(io, value.startsAtMs) && Transfer(io, value.endsAtMs)
                && Transfer(io, value.rules) && TransferList(io, value.players, MaxPlayers);
        }
        bool Transfer(PacketIO& io, FireRequest& value)
        {
            return Transfer(io, value.context)
                && Transfer(io, value.membershipId)
                && Transfer(io, value.shotId)
                && Transfer(io, value.inputSequence)
                && Transfer(io, value.shooterTransform)
                && Transfer(io, value.origin);
        }
        bool Transfer(PacketIO& io, ShotResolved& value)
        {
            return Transfer(io, value.version)
                && Transfer(io, value.shooterToken)
                && Transfer(io, value.shotId)
                && Transfer(io, value.outcome)
                && Transfer(io, value.shooterStats);
        }
        bool Transfer(PacketIO& io, MatchTime& value)
        {
            return Transfer(io, value.context)
                && Transfer(io, value.serverTimeMs)
                && Transfer(io, value.endsAtMs);
        }
        bool Transfer(PacketIO& io, GameResults& value)
        {
            return Transfer(io, value.room) && Transfer(io, value.endedAtMs)
                && Transfer(io, value.reason) && TransferList(io, value.players, MaxPlayers);
        }
        bool Transfer(PacketIO&, UdpRegisterRequest&)
        {
            return true;
        }
        bool Transfer(PacketIO& io, UdpRegisterResult& value)
        {
            return Transfer(io, value.result)
                && Transfer(io, value.registrationSequence);
        }
        bool Transfer(PacketIO& io, PlayerTransform& value)
        {
            return Transfer(io, value.inputSequence)
                && Transfer(io, value.pose);
        }
        template<class T, std::size_t Count>
        bool TransferArray(PacketIO& io, std::array<T, Count>& values, std::size_t count)
        {
            if (count > values.size())
                return false;
            for (std::size_t i = 0; i < count; ++i)
                if (!Transfer(io, values[i]))
                    return false;
            return true;
        }
        bool Transfer(PacketIO& io, PlayerSnapshot& value)
        {
            return Transfer(io, value.serverTimeMs) && Transfer(io, value.requiredRevision)
                && Transfer(io, value.playerCount) && TransferArray(io, value.players, value.playerCount);
        }
        bool Transfer(PacketIO& io, TargetSnapshot& value)
        {
            return Transfer(io, value.targetCount) && TransferArray(io, value.targets, value.targetCount);
        }

        // 출력 본문은 임시값으로 처리하고 검증이 끝난 뒤에만 반영한다.
        template<class T>
        bool EncodeBodyFields(const T& value, std::vector<std::uint8_t>& bytes)
        {
            if (!PacketCodec::ValidateBody(value))
                return false;
            T candidate = value;
            PacketIO io(bytes);
            return Transfer(io, candidate);
        }
        template<class Type, class T>
        bool DecodeBodyFields(Type type, const std::uint8_t* data, std::size_t size, T& output)
        {
            const std::size_t limit = PacketTraits<T>::transport == PacketTransport::Tcp
                ? MaxTcpPacketBytes - TcpHeaderBytes : MaxUdpPacketBytes - UdpHeaderBytes;
            if (type != PacketTraits<T>::type || !CanReadBody(data, size, limit))
                return false;
            T candidate{};
            PacketIO io(data, size);
            if (!Transfer(io, candidate) || io.offset != size || !PacketCodec::ValidateBody(candidate))
                return false;
            output = candidate;
            return true;
        }

        // 기존 값 범위와 본문 검증 정책을 유지한다.
        bool Valid(ErrorCode value) { return value <= ErrorCode::ServerFull && static_cast<std::uint16_t>(value) != 2; }
        bool Valid(GameState value) { return value >= GameState::Preparing && value <= GameState::Results; }
        bool Valid(ReadyState value) { return value >= ReadyState::Connecting && value <= ReadyState::Playing; }
        bool Valid(ShotOutcome value) { return value >= ShotOutcome::Miss && value <= ShotOutcome::BlockedByWorld; }
        bool Valid(MatchEndReason value) { return value >= MatchEndReason::TimeLimit && value <= MatchEndReason::ServerStopped; }
        bool Valid(const MatchRules& value) { return value.activeTargetCount <= MaxTargets; }
        bool Valid(const PlayerData& value) { return Valid(value.ready) && value.mapLoaded <= 1 && IsFinitePlayerPose(value.pose); }
        bool Valid(const RoomInfo& value)
        {
            return value.capacity <= MaxPlayers && value.playerCount <= MaxPlayers && Valid(value.state);
        }
        bool ValidPlayers(const std::vector<PlayerData>& players)
        {
            if (players.size() > MaxPlayers)
            {
                return false;
            }
            for (std::size_t i = 0; i < players.size(); ++i)
            {
                if (!Valid(players[i]))
                {
                    return false;
                }
            }
            return true;
        }
        bool Valid(const RoomStateData& value)
        {
            return value.capacity <= MaxPlayers && Valid(value.state) && Valid(value.rules)
                && value.vAreas.size() <= MaxPlayers && ValidPlayers(value.vPlayers);
        }
    }
    // 각 함수가 받는 본문 타입이 정해져 있어 다른 종류의 본문을 섞지 않는다.
    bool PacketCodec::ValidateBody(const ConnectResult& value)
    {
        return Valid(value.result);
    }
    bool PacketCodec::ValidateBody(const RequestRejected& value)
    {
        return Valid(value.reason);
    }
    bool PacketCodec::ValidateBody(const RoomListRequest&)
    {
        return true;
    }
    bool PacketCodec::ValidateBody(const RoomListResult& value)
    {
        if (value.rooms.size() > MaxRooms)
        {
            return false;
        }
        for (std::size_t i = 0; i < value.rooms.size(); ++i)
        {
            if (!Valid(value.rooms[i]))
            {
                return false;
            }
        }
        return true;
    }
    bool PacketCodec::ValidateBody(const CreateRoomRequest&)
    {
        return true;
    }
    bool PacketCodec::ValidateBody(const JoinRoomRequest&)
    {
        return true;
    }
    bool PacketCodec::ValidateBody(const RoomEnterResult& value)
    {
        return Valid(value.result) && (value.result != ErrorCode::Ok || Valid(value.room));
    }
    bool PacketCodec::ValidateBody(const LeaveRoomRequest&)
    {
        return true;
    }
    bool PacketCodec::ValidateBody(const LeaveRoomResult& value)
    {
        return Valid(value.result);
    }
    bool PacketCodec::ValidateBody(const MapLoaded&)
    {
        return true;
    }
    bool PacketCodec::ValidateBody(const ReadyChangeRequest& value)
    {
        return value.ready <= 1;
    }
    bool PacketCodec::ValidateBody(const RoomState& value)
    {
        return Valid(value.room);
    }
    bool PacketCodec::ValidateBody(const GameStarted& value)
    {
        return Valid(value.rules) && ValidPlayers(value.players);
    }
    bool PacketCodec::ValidateBody(const FireRequest&)
    {
        return true;
    }
    bool PacketCodec::ValidateBody(const ShotResolved& value)
    {
        return Valid(value.outcome);
    }
    bool PacketCodec::ValidateBody(const MatchTime&)
    {
        return true;
    }
    bool PacketCodec::ValidateBody(const GameResults& value)
    {
        return Valid(value.room) && Valid(value.reason) && value.players.size() <= MaxPlayers;
    }
    bool PacketCodec::ValidateBody(const UdpRegisterRequest&)
    {
        return true;
    }
    bool PacketCodec::ValidateBody(const UdpRegisterResult& value)
    {
        return Valid(value.result);
    }
    bool PacketCodec::ValidateBody(const PlayerTransform& value)
    {
        return IsFinitePlayerPose(value.pose);
    }
    bool PacketCodec::ValidateBody(const PlayerSnapshot& value)
    {
        if (value.playerCount > MaxPlayers)
            return false;
        for (std::size_t i = 0; i < value.playerCount; ++i)
            if (!IsFinitePlayerPose(value.players[i].pose))
                return false;
        return true;
    }
    bool PacketCodec::ValidateBody(const TargetSnapshot& value)
    {
        return value.targetCount <= MaxTargets;
    }
    bool PacketCodec::EncodeBody(const ConnectResult& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, ConnectResult& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const RequestRejected& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, RequestRejected& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const RoomListRequest& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, RoomListRequest& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const RoomListResult& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, RoomListResult& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const CreateRoomRequest& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, CreateRoomRequest& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const JoinRoomRequest& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, JoinRoomRequest& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const RoomEnterResult& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, RoomEnterResult& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const LeaveRoomRequest& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, LeaveRoomRequest& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const LeaveRoomResult& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, LeaveRoomResult& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const MapLoaded& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, MapLoaded& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const ReadyChangeRequest& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, ReadyChangeRequest& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const RoomState& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, RoomState& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const GameStarted& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, GameStarted& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const FireRequest& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, FireRequest& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const ShotResolved& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, ShotResolved& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const MatchTime& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, MatchTime& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const GameResults& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, GameResults& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const UdpRegisterRequest& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(UdpType type, const std::uint8_t* data,
        std::size_t size, UdpRegisterRequest& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const UdpRegisterResult& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(UdpType type, const std::uint8_t* data,
        std::size_t size, UdpRegisterResult& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const PlayerTransform& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(UdpType type, const std::uint8_t* data,
        std::size_t size, PlayerTransform& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const PlayerSnapshot& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(UdpType type, const std::uint8_t* data,
        std::size_t size, PlayerSnapshot& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::EncodeBody(const TargetSnapshot& value, std::vector<std::uint8_t>& bytes)
    {
        return EncodeBodyFields(value, bytes);
    }
    bool PacketCodec::DecodeBody(UdpType type, const std::uint8_t* data,
        std::size_t size, TargetSnapshot& output)
    {
        return DecodeBodyFields(type, data, size, output);
    }
    bool PacketCodec::ValidateHeader(const TcpHeader& header)
    {
        if (header.size < TcpHeaderBytes || header.size > MaxTcpPacketBytes)
        {
            return false;
        }
        switch (header.type)
        {
        case TcpType::ConnectResult:
        case TcpType::RequestRejected:
        case TcpType::RoomListRequest:
        case TcpType::RoomListResult:
        case TcpType::CreateRoomRequest:
        case TcpType::JoinRoomRequest:
        case TcpType::RoomEnterResult:
        case TcpType::LeaveRoomRequest:
        case TcpType::LeaveRoomResult:
        case TcpType::MapLoaded:
        case TcpType::ReadyChangeRequest:
        case TcpType::RoomState:
        case TcpType::GameStarted:
        case TcpType::FireRequest:
        case TcpType::ShotResolved:
        case TcpType::MatchTime:
        case TcpType::GameResults:
            return true;
        default:
            return false;
        }
    }
    bool PacketCodec::EncodeTcpFrame(const TcpHeader& header,
        const std::vector<std::uint8_t>& bytes, PacketData& output)
    {
        if (!ValidateHeader(header) || bytes.size() > MaxTcpPacketBytes - TcpHeaderBytes
            || header.size != TcpHeaderBytes + bytes.size())
        {
            return false;
        }
        PacketData packet;
        packet.transport = PacketTransport::Tcp;
        TcpHeader candidate = header;
        PacketIO io(packet.bytes);
        if (!Transfer(io, candidate))
            return false;
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            packet.bytes.push_back(bytes[i]);
        }
        output = packet;
        return true;
    }
    bool PacketCodec::ValidateHeader(const UdpHeader& header)
    {
        if (header.magic != UdpMagic)
        {
            return false;
        }
        switch (header.type)
        {
        case UdpType::RegisterRequest:
        case UdpType::RegisterResult:
        case UdpType::PlayerTransform:
        case UdpType::PlayerSnapshot:
        case UdpType::TargetSnapshot:
            return true;
        default:
            return false;
        }
    }
    bool PacketCodec::EncodeUdpFrame(const UdpHeader& header,
        const std::vector<std::uint8_t>& bytes, PacketData& output)
    {
        if (!ValidateHeader(header) || bytes.size() > MaxUdpPacketBytes - UdpHeaderBytes)
        {
            return false;
        }
        PacketData packet;
        packet.transport = PacketTransport::Udp;
        UdpHeader candidate = header;
        PacketIO io(packet.bytes);
        if (!Transfer(io, candidate))
            return false;
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            packet.bytes.push_back(bytes[i]);
        }
        output = packet;
        return true;
    }
    DecodeStatus PacketCodec::ReadTcpHeader(const std::uint8_t* data, std::size_t size, TcpHeader& output)
    {
        return ReadTcpHeader(data, size, output, nullptr);
    }

    DecodeStatus PacketCodec::ReadTcpHeader(const std::uint8_t* data, std::size_t size,
        TcpHeader& output, const TcpType* expected)
    {
        if (data == nullptr && size != 0)
        {
            return DecodeStatus::Invalid;
        }
        if (size < TcpHeaderBytes)
        {
            return DecodeStatus::NeedMore;
        }
        TcpHeader header;
        PacketIO io(data, size);
        if (!Transfer(io, header) || !ValidateHeader(header)
            || (expected != nullptr && header.type != *expected))
        {
            return DecodeStatus::Invalid;
        }
        if (size < header.size)
        {
            return DecodeStatus::NeedMore;
        }
        output = header;
        return DecodeStatus::Complete;
    }

    bool PacketCodec::ReadUdpHeader(const std::uint8_t* data, std::size_t size,
        UdpHeader& output)
    {
        if (data == nullptr || size < UdpHeaderBytes || size > MaxUdpPacketBytes)
        {
            return false;
        }
        UdpHeader header;
        PacketIO io(data, size);
        if (!Transfer(io, header) || !ValidateHeader(header))
        {
            return false;
        }
        output = header;
        return true;
    }
}
