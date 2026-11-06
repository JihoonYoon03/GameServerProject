#include "PacketCodec.h"
#include <cstring>
#include <limits>

namespace Net
{
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);

    // 큰 자릿수 바이트부터 기록한다. 구조체 전체를 메모리 복사해 보내지 않는다.
    void WriteU8(std::vector<std::uint8_t>& bytes, std::uint8_t value)
    {
        bytes.push_back(value);
    }
    void WriteU16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
    {
        for (int shift = 8; shift >= 0; shift -= 8)
        {
            bytes.push_back(static_cast<std::uint8_t>(value >> shift));
        }
    }
    void WriteU32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
    {
        for (int shift = 24; shift >= 0; shift -= 8)
        {
            bytes.push_back(static_cast<std::uint8_t>(value >> shift));
        }
    }
    void WriteU64(std::vector<std::uint8_t>& bytes, std::uint64_t value)
    {
        for (int shift = 56; shift >= 0; shift -= 8)
        {
            bytes.push_back(static_cast<std::uint8_t>(value >> shift));
        }
    }
    void WriteF32(std::vector<std::uint8_t>& bytes, float value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        WriteU32(bytes, bits);
    }

    // 남은 길이를 먼저 확인하므로 읽기 실패 시 위치와 출력값은 그대로다.
    bool ReadU8(const std::uint8_t* data, std::size_t size,
        std::size_t& offset, std::uint8_t& value)
    {
        if (data == nullptr || offset > size || 1 > size - offset)
        {
            return false;
        }
        std::uint8_t result = 0;
        for (std::size_t i = 0; i < 1; ++i)
        {
            result = static_cast<std::uint8_t>((result << 8) | data[offset + i]);
        }
        offset += 1;
        value = result;
        return true;
    }
    bool ReadU16(const std::uint8_t* data, std::size_t size,
        std::size_t& offset, std::uint16_t& value)
    {
        if (data == nullptr || offset > size || 2 > size - offset)
        {
            return false;
        }
        std::uint16_t result = 0;
        for (std::size_t i = 0; i < 2; ++i)
        {
            result = static_cast<std::uint16_t>((result << 8) | data[offset + i]);
        }
        offset += 2;
        value = result;
        return true;
    }
    bool ReadU32(const std::uint8_t* data, std::size_t size,
        std::size_t& offset, std::uint32_t& value)
    {
        if (data == nullptr || offset > size || 4 > size - offset)
        {
            return false;
        }
        std::uint32_t result = 0;
        for (std::size_t i = 0; i < 4; ++i)
        {
            result = static_cast<std::uint32_t>((result << 8) | data[offset + i]);
        }
        offset += 4;
        value = result;
        return true;
    }
    bool ReadU64(const std::uint8_t* data, std::size_t size,
        std::size_t& offset, std::uint64_t& value)
    {
        if (data == nullptr || offset > size || 8 > size - offset)
        {
            return false;
        }
        std::uint64_t result = 0;
        for (std::size_t i = 0; i < 8; ++i)
        {
            result = static_cast<std::uint64_t>((result << 8) | data[offset + i]);
        }
        offset += 8;
        value = result;
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
        // 본문 타입마다 필드를 선언된 순서대로 읽고 쓴다.

        void WriteSessionToken(std::vector<std::uint8_t>& bytes, const SessionToken& value)
        {
            for (std::size_t i = 0; i < SessionTokenBytes; ++i)
            {
                WriteU8(bytes, value[i]);
            }
        }

        bool ReadSessionToken(const std::uint8_t* data, std::size_t size, std::size_t& offset, SessionToken& value)
        {
            for (std::size_t i = 0; i < SessionTokenBytes; ++i)
            {
                if (!ReadU8(data, size, offset, value[i]))
                {
                    return false;
                }
            }
            return true;
        }
        void WriteMat4Data(std::vector<std::uint8_t>& bytes, const Mat4Data& value)
        {
            for (std::size_t i = 0; i < 16; ++i)
            {
                WriteF32(bytes, value.elements[i]);
            }
        }

        bool ReadMat4Data(const std::uint8_t* data, std::size_t size, std::size_t& offset, Mat4Data& value)
        {
            for (std::size_t i = 0; i < 16; ++i)
            {
                if (!ReadF32(data, size, offset, value.elements[i]))
                {
                    return false;
                }
            }
            return true;
        }
        void WriteVec3Data(std::vector<std::uint8_t>& bytes, const Vec3Data& value)
        {
            WriteF32(bytes, value.x);
            WriteF32(bytes, value.y);
            WriteF32(bytes, value.z);
        }
        bool ReadVec3Data(const std::uint8_t* data, std::size_t size, std::size_t& offset, Vec3Data& value)
        {
            if (!ReadF32(data, size, offset, value.x))
            {
                return false;
            }
            if (!ReadF32(data, size, offset, value.y))
            {
                return false;
            }
            if (!ReadF32(data, size, offset, value.z))
            {
                return false;
            }
            return true;
        }
        void WriteBoxData(std::vector<std::uint8_t>& bytes, const BoxData& value)
        {
            WriteVec3Data(bytes, value.min);
            WriteVec3Data(bytes, value.max);
        }
        bool ReadBoxData(const std::uint8_t* data, std::size_t size, std::size_t& offset, BoxData& value)
        {
            if (!ReadVec3Data(data, size, offset, value.min))
            {
                return false;
            }
            if (!ReadVec3Data(data, size, offset, value.max))
            {
                return false;
            }
            return true;
        }
        void WriteMatchContext(std::vector<std::uint8_t>& bytes, const MatchContext& value)
        {
            WriteU32(bytes, value.roomId);
            WriteU32(bytes, value.matchId);
        }
        bool ReadMatchContext(const std::uint8_t* data, std::size_t size, std::size_t& offset, MatchContext& value)
        {
            if (!ReadU32(data, size, offset, value.roomId))
            {
                return false;
            }
            if (!ReadU32(data, size, offset, value.matchId))
            {
                return false;
            }
            return true;
        }
        void WriteRoomVersion(std::vector<std::uint8_t>& bytes, const RoomVersion& value)
        {
            WriteMatchContext(bytes, value.context);
            WriteU32(bytes, value.revision);
        }
        bool ReadRoomVersion(const std::uint8_t* data, std::size_t size, std::size_t& offset, RoomVersion& value)
        {
            if (!ReadMatchContext(data, size, offset, value.context))
            {
                return false;
            }
            if (!ReadU32(data, size, offset, value.revision))
            {
                return false;
            }
            return true;
        }
        void WriteAreaData(std::vector<std::uint8_t>& bytes, const AreaData& value)
        {
            WriteU8(bytes, value.areaId);
            WriteBoxData(bytes, value.movementBounds);
            WriteMat4Data(bytes, value.spawnTransform);
        }
        bool ReadAreaData(const std::uint8_t* data, std::size_t size, std::size_t& offset, AreaData& value)
        {
            if (!ReadU8(data, size, offset, value.areaId))
            {
                return false;
            }
            if (!ReadBoxData(data, size, offset, value.movementBounds))
            {
                return false;
            }
            if (!ReadMat4Data(data, size, offset, value.spawnTransform))
            {
                return false;
            }
            return true;
        }
        void WriteMatchRules(std::vector<std::uint8_t>& bytes, const MatchRules& value)
        {
            WriteU32(bytes, value.durationMs);
            WriteU16(bytes, value.activeTargetCount);
            WriteU16(bytes, value.pointsPerHit);
        }
        bool ReadMatchRules(const std::uint8_t* data, std::size_t size, std::size_t& offset, MatchRules& value)
        {
            if (!ReadU32(data, size, offset, value.durationMs))
            {
                return false;
            }
            if (!ReadU16(data, size, offset, value.activeTargetCount))
            {
                return false;
            }
            if (!ReadU16(data, size, offset, value.pointsPerHit))
            {
                return false;
            }
            return true;
        }
        void WritePlayerStats(std::vector<std::uint8_t>& bytes, const PlayerStats& value)
        {
            WriteU32(bytes, value.score);
            WriteU32(bytes, value.shotsFired);
        }
        bool ReadPlayerStats(const std::uint8_t* data, std::size_t size, std::size_t& offset, PlayerStats& value)
        {
            if (!ReadU32(data, size, offset, value.score))
            {
                return false;
            }
            if (!ReadU32(data, size, offset, value.shotsFired))
            {
                return false;
            }
            return true;
        }
        void WritePlayerData(std::vector<std::uint8_t>& bytes, const PlayerData& value)
        {
            WriteSessionToken(bytes, value.sessionToken);
            WriteMat4Data(bytes, value.worldTransform);
            WriteU8(bytes, value.areaId);
            WriteU8(bytes, static_cast<std::uint8_t>(value.ready));
            WriteU8(bytes, value.mapLoaded);
            WritePlayerStats(bytes, value.stats);
        }
        bool ReadPlayerData(const std::uint8_t* data, std::size_t size, std::size_t& offset, PlayerData& value)
        {
            if (!ReadSessionToken(data, size, offset, value.sessionToken))
            {
                return false;
            }
            if (!ReadMat4Data(data, size, offset, value.worldTransform))
            {
                return false;
            }
            if (!ReadU8(data, size, offset, value.areaId))
            {
                return false;
            }
            std::uint8_t ready = 0;
            if (!ReadU8(data, size, offset, ready))
            {
                return false;
            }
            value.ready = static_cast<ReadyState>(ready);
            if (!ReadU8(data, size, offset, value.mapLoaded))
            {
                return false;
            }
            if (!ReadPlayerStats(data, size, offset, value.stats))
            {
                return false;
            }
            return true;
        }
        void WriteRoomInfo(std::vector<std::uint8_t>& bytes, const RoomInfo& value)
        {
            WriteU32(bytes, value.roomId);
            WriteU8(bytes, value.playerCount);
            WriteU8(bytes, value.capacity);
            WriteU8(bytes, static_cast<std::uint8_t>(value.state));
        }
        bool ReadRoomInfo(const std::uint8_t* data, std::size_t size, std::size_t& offset, RoomInfo& value)
        {
            if (!ReadU32(data, size, offset, value.roomId))
            {
                return false;
            }
            if (!ReadU8(data, size, offset, value.playerCount))
            {
                return false;
            }
            if (!ReadU8(data, size, offset, value.capacity))
            {
                return false;
            }
            std::uint8_t state = 0;
            if (!ReadU8(data, size, offset, state))
            {
                return false;
            }
            value.state = static_cast<GameState>(state);
            return true;
        }
        void WritePlayerResult(std::vector<std::uint8_t>& bytes, const PlayerResult& value)
        {
            WriteSessionToken(bytes, value.sessionToken);
            WritePlayerStats(bytes, value.stats);
            WriteU8(bytes, value.rank);
        }
        bool ReadPlayerResult(const std::uint8_t* data, std::size_t size, std::size_t& offset, PlayerResult& value)
        {
            if (!ReadSessionToken(data, size, offset, value.sessionToken))
            {
                return false;
            }
            if (!ReadPlayerStats(data, size, offset, value.stats))
            {
                return false;
            }
            if (!ReadU8(data, size, offset, value.rank))
            {
                return false;
            }
            return true;
        }
        void WriteRoomStateData(std::vector<std::uint8_t>& bytes, const RoomStateData& value)
        {
            WriteRoomVersion(bytes, value.version);
            WriteU8(bytes, static_cast<std::uint8_t>(value.state));
            WriteU8(bytes, value.capacity);
            WriteMatchRules(bytes, value.rules);
            WriteU16(bytes, static_cast<std::uint16_t>(value.areas.size()));
            for (std::size_t i = 0; i < value.areas.size(); ++i)
            {
                WriteAreaData(bytes, value.areas[i]);
            }
            WriteU16(bytes, static_cast<std::uint16_t>(value.players.size()));
            for (std::size_t i = 0; i < value.players.size(); ++i)
            {
                WritePlayerData(bytes, value.players[i]);
            }
        }
        bool ReadRoomStateData(const std::uint8_t* data, std::size_t size, std::size_t& offset, RoomStateData& value)
        {
            if (!ReadRoomVersion(data, size, offset, value.version))
            {
                return false;
            }
            std::uint8_t state = 0;
            if (!ReadU8(data, size, offset, state))
            {
                return false;
            }
            value.state = static_cast<GameState>(state);
            if (!ReadU8(data, size, offset, value.capacity))
            {
                return false;
            }
            if (!ReadMatchRules(data, size, offset, value.rules))
            {
                return false;
            }
            std::uint16_t areasCount = 0;
            if (!ReadU16(data, size, offset, areasCount) || areasCount > MaxPlayers)
            {
                return false;
            }
            value.areas.resize(areasCount);
            for (std::size_t i = 0; i < value.areas.size(); ++i)
            {
                if (!ReadAreaData(data, size, offset, value.areas[i]))
                {
                    return false;
                }
            }
            std::uint16_t playersCount = 0;
            if (!ReadU16(data, size, offset, playersCount) || playersCount > MaxPlayers)
            {
                return false;
            }
            value.players.resize(playersCount);
            for (std::size_t i = 0; i < value.players.size(); ++i)
            {
                if (!ReadPlayerData(data, size, offset, value.players[i]))
                {
                    return false;
                }
            }
            return true;
        }
        void WritePlayerTransformEntry(std::vector<std::uint8_t>& bytes, const PlayerTransformEntry& value)
        {
            WriteSessionToken(bytes, value.sessionToken);
            WriteMat4Data(bytes, value.worldTransform);
            WriteU32(bytes, value.lastProcessedInput);
        }
        bool ReadPlayerTransformEntry(const std::uint8_t* data, std::size_t size, std::size_t& offset, PlayerTransformEntry& value)
        {
            if (!ReadSessionToken(data, size, offset, value.sessionToken))
            {
                return false;
            }
            if (!ReadMat4Data(data, size, offset, value.worldTransform))
            {
                return false;
            }
            if (!ReadU32(data, size, offset, value.lastProcessedInput))
            {
                return false;
            }
            return true;
        }
        void WriteTargetMotionData(std::vector<std::uint8_t>& bytes, const TargetMotionData& value)
        {
            WriteU32(bytes, value.targetId);
            WriteVec3Data(bytes, value.position);
            WriteVec3Data(bytes, value.velocity);
        }
        bool ReadTargetMotionData(const std::uint8_t* data, std::size_t size, std::size_t& offset, TargetMotionData& value)
        {
            if (!ReadU32(data, size, offset, value.targetId))
            {
                return false;
            }
            if (!ReadVec3Data(data, size, offset, value.position))
            {
                return false;
            }
            if (!ReadVec3Data(data, size, offset, value.velocity))
            {
                return false;
            }
            return true;
        }
        void WriteTcpHeader(std::vector<std::uint8_t>& bytes, const TcpHeader& value)
        {
            WriteU32(bytes, value.size);
            WriteU16(bytes, value.version);
            WriteU16(bytes, static_cast<std::uint16_t>(value.type));
            WriteU32(bytes, value.requestId);
        }
        bool ReadTcpFields(const std::uint8_t* data, std::size_t size, std::size_t& offset, TcpHeader& value)
        {
            if (!ReadU32(data, size, offset, value.size))
            {
                return false;
            }
            if (!ReadU16(data, size, offset, value.version))
            {
                return false;
            }
            std::uint16_t type = 0;
            if (!ReadU16(data, size, offset, type))
            {
                return false;
            }
            value.type = static_cast<TcpType>(type);
            if (!ReadU32(data, size, offset, value.requestId))
            {
                return false;
            }
            return true;
        }
        void WriteUdpHeader(std::vector<std::uint8_t>& bytes, const UdpHeader& value)
        {
            WriteU16(bytes, value.magic);
            WriteU16(bytes, value.version);
            WriteU16(bytes, static_cast<std::uint16_t>(value.type));
            WriteU16(bytes, value.size);
            WriteSessionToken(bytes, value.sessionToken);
            WriteU32(bytes, value.sequence);
            WriteMatchContext(bytes, value.context);
            WriteU32(bytes, value.membershipId);
            WriteU32(bytes, value.serverTick);
        }
        bool ReadUdpFields(const std::uint8_t* data, std::size_t size, std::size_t& offset, UdpHeader& value)
        {
            if (!ReadU16(data, size, offset, value.magic))
            {
                return false;
            }
            if (!ReadU16(data, size, offset, value.version))
            {
                return false;
            }
            std::uint16_t type = 0;
            if (!ReadU16(data, size, offset, type))
            {
                return false;
            }
            value.type = static_cast<UdpType>(type);
            if (!ReadU16(data, size, offset, value.size))
            {
                return false;
            }
            if (!ReadSessionToken(data, size, offset, value.sessionToken))
            {
                return false;
            }
            if (!ReadU32(data, size, offset, value.sequence))
            {
                return false;
            }
            if (!ReadMatchContext(data, size, offset, value.context))
            {
                return false;
            }
            if (!ReadU32(data, size, offset, value.membershipId))
            {
                return false;
            }
            if (!ReadU32(data, size, offset, value.serverTick))
            {
                return false;
            }
            return true;
        }
        void WriteConnectResult(std::vector<std::uint8_t>& bytes, const ConnectResult& value)
        {
            WriteU16(bytes, static_cast<std::uint16_t>(value.result));
            WriteSessionToken(bytes, value.sessionToken);
            WriteU16(bytes, value.udpPort);
            WriteU64(bytes, value.serverTimeMs);
            WriteU16(bytes, value.tickRate);
        }
        bool ReadConnectResult(const std::uint8_t* data, std::size_t size, std::size_t& offset, ConnectResult& value)
        {
            std::uint16_t result = 0;
            if (!ReadU16(data, size, offset, result))
            {
                return false;
            }
            value.result = static_cast<ErrorCode>(result);
            if (!ReadSessionToken(data, size, offset, value.sessionToken))
            {
                return false;
            }
            if (!ReadU16(data, size, offset, value.udpPort))
            {
                return false;
            }
            if (!ReadU64(data, size, offset, value.serverTimeMs))
            {
                return false;
            }
            if (!ReadU16(data, size, offset, value.tickRate))
            {
                return false;
            }
            return true;
        }
        void WriteRequestRejected(std::vector<std::uint8_t>& bytes, const RequestRejected& value)
        {
            WriteU16(bytes, static_cast<std::uint16_t>(value.reason));
            WriteMatchContext(bytes, value.context);
        }
        bool ReadRequestRejected(const std::uint8_t* data, std::size_t size, std::size_t& offset, RequestRejected& value)
        {
            std::uint16_t reason = 0;
            if (!ReadU16(data, size, offset, reason))
            {
                return false;
            }
            value.reason = static_cast<ErrorCode>(reason);
            if (!ReadMatchContext(data, size, offset, value.context))
            {
                return false;
            }
            return true;
        }
        void WriteRoomListRequest(std::vector<std::uint8_t>& bytes, const RoomListRequest& value)
        {
            WriteU32(bytes, value.afterRoomId);
        }
        bool ReadRoomListRequest(const std::uint8_t* data, std::size_t size, std::size_t& offset, RoomListRequest& value)
        {
            if (!ReadU32(data, size, offset, value.afterRoomId))
            {
                return false;
            }
            return true;
        }
        void WriteRoomListResult(std::vector<std::uint8_t>& bytes, const RoomListResult& value)
        {
            WriteU32(bytes, value.nextAfterRoomId);
            WriteU16(bytes, static_cast<std::uint16_t>(value.rooms.size()));
            for (std::size_t i = 0; i < value.rooms.size(); ++i)
            {
                WriteRoomInfo(bytes, value.rooms[i]);
            }
        }
        bool ReadRoomListResult(const std::uint8_t* data, std::size_t size, std::size_t& offset, RoomListResult& value)
        {
            if (!ReadU32(data, size, offset, value.nextAfterRoomId))
            {
                return false;
            }
            std::uint16_t roomsCount = 0;
            if (!ReadU16(data, size, offset, roomsCount) || roomsCount > MaxRoomsPerPage)
            {
                return false;
            }
            value.rooms.resize(roomsCount);
            for (std::size_t i = 0; i < value.rooms.size(); ++i)
            {
                if (!ReadRoomInfo(data, size, offset, value.rooms[i]))
                {
                    return false;
                }
            }
            return true;
        }
        void WriteCreateRoomRequest(std::vector<std::uint8_t>&, const CreateRoomRequest&)
        {
        }
        bool ReadCreateRoomRequest(const std::uint8_t*, std::size_t, std::size_t&, CreateRoomRequest&)
        {
            return true;
        }
        void WriteJoinRoomRequest(std::vector<std::uint8_t>& bytes, const JoinRoomRequest& value)
        {
            WriteU32(bytes, value.roomId);
        }
        bool ReadJoinRoomRequest(const std::uint8_t* data, std::size_t size, std::size_t& offset, JoinRoomRequest& value)
        {
            if (!ReadU32(data, size, offset, value.roomId))
            {
                return false;
            }
            return true;
        }
        void WriteRoomEnterResult(std::vector<std::uint8_t>& bytes, const RoomEnterResult& value)
        {
            WriteU16(bytes, static_cast<std::uint16_t>(value.result));
            if (value.result != ErrorCode::Ok)
            {
                return;
            }
            WriteU32(bytes, value.membershipId);
            WriteRoomStateData(bytes, value.room);
        }
        bool ReadRoomEnterResult(const std::uint8_t* data, std::size_t size, std::size_t& offset, RoomEnterResult& value)
        {
            std::uint16_t result = 0;
            if (!ReadU16(data, size, offset, result))
            {
                return false;
            }
            value.result = static_cast<ErrorCode>(result);
            if (value.result != ErrorCode::Ok)
            {
                return true;
            }
            if (!ReadU32(data, size, offset, value.membershipId))
            {
                return false;
            }
            if (!ReadRoomStateData(data, size, offset, value.room))
            {
                return false;
            }
            return true;
        }
        void WriteLeaveRoomRequest(std::vector<std::uint8_t>& bytes, const LeaveRoomRequest& value)
        {
            WriteU32(bytes, value.roomId);
        }
        bool ReadLeaveRoomRequest(const std::uint8_t* data, std::size_t size, std::size_t& offset, LeaveRoomRequest& value)
        {
            if (!ReadU32(data, size, offset, value.roomId))
            {
                return false;
            }
            return true;
        }
        void WriteLeaveRoomResult(std::vector<std::uint8_t>& bytes, const LeaveRoomResult& value)
        {
            WriteU16(bytes, static_cast<std::uint16_t>(value.result));
            WriteU32(bytes, value.roomId);
        }
        bool ReadLeaveRoomResult(const std::uint8_t* data, std::size_t size, std::size_t& offset, LeaveRoomResult& value)
        {
            std::uint16_t result = 0;
            if (!ReadU16(data, size, offset, result))
            {
                return false;
            }
            value.result = static_cast<ErrorCode>(result);
            if (!ReadU32(data, size, offset, value.roomId))
            {
                return false;
            }
            return true;
        }
        void WriteMapLoaded(std::vector<std::uint8_t>& bytes, const MapLoaded& value)
        {
            WriteU32(bytes, value.roomId);
        }
        bool ReadMapLoaded(const std::uint8_t* data, std::size_t size, std::size_t& offset, MapLoaded& value)
        {
            if (!ReadU32(data, size, offset, value.roomId))
            {
                return false;
            }
            return true;
        }
        void WriteReadyChangeRequest(std::vector<std::uint8_t>& bytes, const ReadyChangeRequest& value)
        {
            WriteU32(bytes, value.roomId);
            WriteU8(bytes, value.ready);
        }
        bool ReadReadyChangeRequest(const std::uint8_t* data, std::size_t size, std::size_t& offset, ReadyChangeRequest& value)
        {
            if (!ReadU32(data, size, offset, value.roomId))
            {
                return false;
            }
            if (!ReadU8(data, size, offset, value.ready))
            {
                return false;
            }
            return true;
        }
        void WriteRoomState(std::vector<std::uint8_t>& bytes, const RoomState& value)
        {
            WriteRoomStateData(bytes, value.room);
        }
        bool ReadRoomState(const std::uint8_t* data, std::size_t size, std::size_t& offset, RoomState& value)
        {
            if (!ReadRoomStateData(data, size, offset, value.room))
            {
                return false;
            }
            return true;
        }
        void WriteGameStarted(std::vector<std::uint8_t>& bytes, const GameStarted& value)
        {
            WriteRoomVersion(bytes, value.version);
            WriteU64(bytes, value.serverTimeMs);
            WriteU64(bytes, value.startsAtMs);
            WriteU64(bytes, value.endsAtMs);
            WriteMatchRules(bytes, value.rules);
            WriteU16(bytes, static_cast<std::uint16_t>(value.players.size()));
            for (std::size_t i = 0; i < value.players.size(); ++i)
            {
                WritePlayerData(bytes, value.players[i]);
            }
        }
        bool ReadGameStarted(const std::uint8_t* data, std::size_t size, std::size_t& offset, GameStarted& value)
        {
            if (!ReadRoomVersion(data, size, offset, value.version))
            {
                return false;
            }
            if (!ReadU64(data, size, offset, value.serverTimeMs))
            {
                return false;
            }
            if (!ReadU64(data, size, offset, value.startsAtMs))
            {
                return false;
            }
            if (!ReadU64(data, size, offset, value.endsAtMs))
            {
                return false;
            }
            if (!ReadMatchRules(data, size, offset, value.rules))
            {
                return false;
            }
            std::uint16_t playersCount = 0;
            if (!ReadU16(data, size, offset, playersCount) || playersCount > MaxPlayers)
            {
                return false;
            }
            value.players.resize(playersCount);
            for (std::size_t i = 0; i < value.players.size(); ++i)
            {
                if (!ReadPlayerData(data, size, offset, value.players[i]))
                {
                    return false;
                }
            }
            return true;
        }
        void WriteFireRequest(std::vector<std::uint8_t>& bytes, const FireRequest& value)
        {
            WriteMatchContext(bytes, value.context);
            WriteU32(bytes, value.membershipId);
            WriteU32(bytes, value.shotId);
            WriteU32(bytes, value.inputSequence);
            WriteMat4Data(bytes, value.shooterTransform);
            WriteVec3Data(bytes, value.origin);
        }
        bool ReadFireRequest(const std::uint8_t* data, std::size_t size, std::size_t& offset, FireRequest& value)
        {
            if (!ReadMatchContext(data, size, offset, value.context))
            {
                return false;
            }
            if (!ReadU32(data, size, offset, value.membershipId))
            {
                return false;
            }
            if (!ReadU32(data, size, offset, value.shotId))
            {
                return false;
            }
            if (!ReadU32(data, size, offset, value.inputSequence))
            {
                return false;
            }
            if (!ReadMat4Data(data, size, offset, value.shooterTransform))
            {
                return false;
            }
            if (!ReadVec3Data(data, size, offset, value.origin))
            {
                return false;
            }
            return true;
        }
        void WriteShotResolved(std::vector<std::uint8_t>& bytes, const ShotResolved& value)
        {
            WriteRoomVersion(bytes, value.version);
            WriteSessionToken(bytes, value.shooterToken);
            WriteU32(bytes, value.shotId);
            WriteU8(bytes, static_cast<std::uint8_t>(value.outcome));
            WriteU32(bytes, value.hitTargetId);
            WritePlayerStats(bytes, value.shooterStats);
        }
        bool ReadShotResolved(const std::uint8_t* data, std::size_t size, std::size_t& offset, ShotResolved& value)
        {
            if (!ReadRoomVersion(data, size, offset, value.version))
            {
                return false;
            }
            if (!ReadSessionToken(data, size, offset, value.shooterToken))
            {
                return false;
            }
            if (!ReadU32(data, size, offset, value.shotId))
            {
                return false;
            }
            std::uint8_t outcome = 0;
            if (!ReadU8(data, size, offset, outcome))
            {
                return false;
            }
            value.outcome = static_cast<ShotOutcome>(outcome);
            if (!ReadU32(data, size, offset, value.hitTargetId))
            {
                return false;
            }
            if (!ReadPlayerStats(data, size, offset, value.shooterStats))
            {
                return false;
            }
            return true;
        }
        void WriteMatchTime(std::vector<std::uint8_t>& bytes, const MatchTime& value)
        {
            WriteMatchContext(bytes, value.context);
            WriteU64(bytes, value.serverTimeMs);
            WriteU64(bytes, value.endsAtMs);
        }
        bool ReadMatchTime(const std::uint8_t* data, std::size_t size, std::size_t& offset, MatchTime& value)
        {
            if (!ReadMatchContext(data, size, offset, value.context))
            {
                return false;
            }
            if (!ReadU64(data, size, offset, value.serverTimeMs))
            {
                return false;
            }
            if (!ReadU64(data, size, offset, value.endsAtMs))
            {
                return false;
            }
            return true;
        }
        void WriteGameResults(std::vector<std::uint8_t>& bytes, const GameResults& value)
        {
            WriteRoomStateData(bytes, value.room);
            WriteU64(bytes, value.endedAtMs);
            WriteU8(bytes, static_cast<std::uint8_t>(value.reason));
            WriteU16(bytes, static_cast<std::uint16_t>(value.players.size()));
            for (std::size_t i = 0; i < value.players.size(); ++i)
            {
                WritePlayerResult(bytes, value.players[i]);
            }
        }
        bool ReadGameResults(const std::uint8_t* data, std::size_t size, std::size_t& offset, GameResults& value)
        {
            if (!ReadRoomStateData(data, size, offset, value.room))
            {
                return false;
            }
            if (!ReadU64(data, size, offset, value.endedAtMs))
            {
                return false;
            }
            std::uint8_t reason = 0;
            if (!ReadU8(data, size, offset, reason))
            {
                return false;
            }
            value.reason = static_cast<MatchEndReason>(reason);
            std::uint16_t playersCount = 0;
            if (!ReadU16(data, size, offset, playersCount) || playersCount > MaxPlayers)
            {
                return false;
            }
            value.players.resize(playersCount);
            for (std::size_t i = 0; i < value.players.size(); ++i)
            {
                if (!ReadPlayerResult(data, size, offset, value.players[i]))
                {
                    return false;
                }
            }
            return true;
        }
        void WriteReloadRequest(std::vector<std::uint8_t>& bytes, const ReloadRequest& value)
        {
            WriteMatchContext(bytes, value.context);
            WriteU32(bytes, value.membershipId);
        }
        bool ReadReloadRequest(const std::uint8_t* data, std::size_t size, std::size_t& offset, ReloadRequest& value)
        {
            if (!ReadMatchContext(data, size, offset, value.context))
            {
                return false;
            }
            if (!ReadU32(data, size, offset, value.membershipId))
            {
                return false;
            }
            return true;
        }
        void WriteWeaponState(std::vector<std::uint8_t>& bytes, const WeaponState& value)
        {
            WriteRoomVersion(bytes, value.version);
            WriteSessionToken(bytes, value.playerToken);
            WriteU8(bytes, value.ammo);
            WriteU8(bytes, value.reloading);
            WriteU64(bytes, value.nextFireAtMs);
            WriteU64(bytes, value.reloadEndsAtMs);
        }
        bool ReadWeaponState(const std::uint8_t* data, std::size_t size, std::size_t& offset, WeaponState& value)
        {
            if (!ReadRoomVersion(data, size, offset, value.version))
            {
                return false;
            }
            if (!ReadSessionToken(data, size, offset, value.playerToken))
            {
                return false;
            }
            if (!ReadU8(data, size, offset, value.ammo))
            {
                return false;
            }
            if (!ReadU8(data, size, offset, value.reloading))
            {
                return false;
            }
            if (!ReadU64(data, size, offset, value.nextFireAtMs))
            {
                return false;
            }
            if (!ReadU64(data, size, offset, value.reloadEndsAtMs))
            {
                return false;
            }
            return true;
        }
        void WriteTimeProbeRequest(std::vector<std::uint8_t>& bytes, const TimeProbeRequest& value)
        {
            WriteU64(bytes, value.clientSendMs);
        }
        bool ReadTimeProbeRequest(const std::uint8_t* data, std::size_t size, std::size_t& offset, TimeProbeRequest& value)
        {
            if (!ReadU64(data, size, offset, value.clientSendMs))
            {
                return false;
            }
            return true;
        }
        void WriteTimeProbeResult(std::vector<std::uint8_t>& bytes, const TimeProbeResult& value)
        {
            WriteU64(bytes, value.clientSendMs);
            WriteU64(bytes, value.serverReceiveMs);
            WriteU64(bytes, value.serverSendMs);
        }
        bool ReadTimeProbeResult(const std::uint8_t* data, std::size_t size, std::size_t& offset, TimeProbeResult& value)
        {
            if (!ReadU64(data, size, offset, value.clientSendMs))
            {
                return false;
            }
            if (!ReadU64(data, size, offset, value.serverReceiveMs))
            {
                return false;
            }
            if (!ReadU64(data, size, offset, value.serverSendMs))
            {
                return false;
            }
            return true;
        }
        void WriteUdpRegisterRequest(std::vector<std::uint8_t>&, const UdpRegisterRequest&)
        {
        }
        bool ReadUdpRegisterRequest(const std::uint8_t*, std::size_t, std::size_t&, UdpRegisterRequest&)
        {
            return true;
        }
        void WriteUdpRegisterResult(std::vector<std::uint8_t>& bytes, const UdpRegisterResult& value)
        {
            WriteU16(bytes, static_cast<std::uint16_t>(value.result));
            WriteU32(bytes, value.registrationSequence);
        }
        bool ReadUdpRegisterResult(const std::uint8_t* data, std::size_t size, std::size_t& offset, UdpRegisterResult& value)
        {
            std::uint16_t result = 0;
            if (!ReadU16(data, size, offset, result))
            {
                return false;
            }
            value.result = static_cast<ErrorCode>(result);
            if (!ReadU32(data, size, offset, value.registrationSequence))
            {
                return false;
            }
            return true;
        }
        void WritePlayerTransform(std::vector<std::uint8_t>& bytes, const PlayerTransform& value)
        {
            WriteU32(bytes, value.inputSequence);
            WriteMat4Data(bytes, value.worldTransform);
        }
        bool ReadPlayerTransform(const std::uint8_t* data, std::size_t size, std::size_t& offset, PlayerTransform& value)
        {
            if (!ReadU32(data, size, offset, value.inputSequence))
            {
                return false;
            }
            if (!ReadMat4Data(data, size, offset, value.worldTransform))
            {
                return false;
            }
            return true;
        }
        void WritePlayerSnapshot(std::vector<std::uint8_t>& bytes, const PlayerSnapshot& value)
        {
            WriteU64(bytes, value.serverTimeMs);
            WriteU32(bytes, value.requiredRevision);
            WriteU8(bytes, value.playerCount);
            for (std::size_t i = 0; i < value.playerCount; ++i)
            {
                WritePlayerTransformEntry(bytes, value.players[i]);
            }
        }
        bool ReadPlayerSnapshot(const std::uint8_t* data, std::size_t size, std::size_t& offset, PlayerSnapshot& value)
        {
            if (!ReadU64(data, size, offset, value.serverTimeMs))
            {
                return false;
            }
            if (!ReadU32(data, size, offset, value.requiredRevision))
            {
                return false;
            }
            if (!ReadU8(data, size, offset, value.playerCount))
            {
                return false;
            }
            if (value.playerCount > MaxPlayers)
            {
                return false;
            }
            for (std::size_t i = 0; i < value.playerCount; ++i)
            {
                if (!ReadPlayerTransformEntry(data, size, offset, value.players[i]))
                {
                    return false;
                }
            }
            return true;
        }
        void WriteTargetSnapshot(std::vector<std::uint8_t>& bytes, const TargetSnapshot& value)
        {
            WriteU64(bytes, value.serverTimeMs);
            WriteU16(bytes, value.totalTargetCount);
            WriteU16(bytes, value.chunkIndex);
            WriteU16(bytes, value.chunkCount);
            WriteU16(bytes, value.targetCount);
            for (std::size_t i = 0; i < value.targetCount; ++i)
            {
                WriteTargetMotionData(bytes, value.targets[i]);
            }
        }
        bool ReadTargetSnapshot(const std::uint8_t* data, std::size_t size, std::size_t& offset, TargetSnapshot& value)
        {
            if (!ReadU64(data, size, offset, value.serverTimeMs))
            {
                return false;
            }
            if (!ReadU16(data, size, offset, value.totalTargetCount))
            {
                return false;
            }
            if (!ReadU16(data, size, offset, value.chunkIndex))
            {
                return false;
            }
            if (!ReadU16(data, size, offset, value.chunkCount))
            {
                return false;
            }
            if (!ReadU16(data, size, offset, value.targetCount))
            {
                return false;
            }
            if (value.targetCount > MaxTargetsPerSnapshot)
            {
                return false;
            }
            for (std::size_t i = 0; i < value.targetCount; ++i)
            {
                if (!ReadTargetMotionData(data, size, offset, value.targets[i]))
                {
                    return false;
                }
            }
            return true;
        }
        // 코덱은 전송 형식만 검사한다. 경기 규칙과 행렬 판정은 각 담당 로직에서 처리한다.
        bool Valid(ErrorCode value) { return value >= ErrorCode::Ok && value <= ErrorCode::ServerFull; }
        bool Valid(GameState value) { return value >= GameState::Preparing && value <= GameState::Results; }
        bool Valid(ReadyState value) { return value >= ReadyState::Connecting && value <= ReadyState::Playing; }
        bool Valid(ShotOutcome value) { return value >= ShotOutcome::Miss && value <= ShotOutcome::BlockedByWorld; }
        bool Valid(MatchEndReason value) { return value >= MatchEndReason::TimeLimit && value <= MatchEndReason::ServerStopped; }
        bool Valid(const MatchRules& value) { return value.activeTargetCount <= MaxTargets; }
        bool Valid(const PlayerData& value) { return Valid(value.ready) && value.mapLoaded <= 1; }
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
                && value.areas.size() <= MaxPlayers && ValidPlayers(value.players);
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
        if (value.rooms.size() > MaxRoomsPerPage)
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
    bool PacketCodec::ValidateBody(const ReloadRequest&)
    {
        return true;
    }
    bool PacketCodec::ValidateBody(const WeaponState& value)
    {
        return value.ammo <= MagazineCapacity && value.reloading <= 1;
    }
    bool PacketCodec::ValidateBody(const TimeProbeRequest&)
    {
        return true;
    }
    bool PacketCodec::ValidateBody(const TimeProbeResult&)
    {
        return true;
    }
    bool PacketCodec::ValidateBody(const UdpRegisterRequest&)
    {
        return true;
    }
    bool PacketCodec::ValidateBody(const UdpRegisterResult& value)
    {
        return Valid(value.result);
    }
    bool PacketCodec::ValidateBody(const PlayerTransform&)
    {
        return true;
    }
    bool PacketCodec::ValidateBody(const PlayerSnapshot& value)
    {
        return value.playerCount <= MaxPlayers;
    }
    bool PacketCodec::ValidateBody(const TargetSnapshot& value)
    {
        if (value.totalTargetCount > MaxTargets || value.chunkCount == 0
            || value.chunkCount > MaxTargetSnapshotChunks || value.chunkIndex >= value.chunkCount
            || value.targetCount > MaxTargetsPerSnapshot)
        {
            return false;
        }
        const int chunks = value.totalTargetCount == 0 ? 1
            : (value.totalTargetCount + MaxTargetsPerSnapshot - 1) / MaxTargetsPerSnapshot;
        if (value.chunkCount != chunks)
        {
            return false;
        }
        const int remaining = value.totalTargetCount - value.chunkIndex * MaxTargetsPerSnapshot;
        const int count = remaining < MaxTargetsPerSnapshot ? remaining : MaxTargetsPerSnapshot;
        return value.targetCount == count;
    }
    bool PacketCodec::EncodeBody(const ConnectResult& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteConnectResult(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, ConnectResult& output)
    {
        if (type != TcpType::ConnectResult || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        ConnectResult value{};
        std::size_t offset = 0;
        if (!ReadConnectResult(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const RequestRejected& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteRequestRejected(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, RequestRejected& output)
    {
        if (type != TcpType::RequestRejected || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        RequestRejected value{};
        std::size_t offset = 0;
        if (!ReadRequestRejected(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const RoomListRequest& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteRoomListRequest(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, RoomListRequest& output)
    {
        if (type != TcpType::RoomListRequest || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        RoomListRequest value{};
        std::size_t offset = 0;
        if (!ReadRoomListRequest(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const RoomListResult& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteRoomListResult(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, RoomListResult& output)
    {
        if (type != TcpType::RoomListResult || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        RoomListResult value{};
        std::size_t offset = 0;
        if (!ReadRoomListResult(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const CreateRoomRequest& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteCreateRoomRequest(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, CreateRoomRequest& output)
    {
        if (type != TcpType::CreateRoomRequest || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        CreateRoomRequest value{};
        std::size_t offset = 0;
        if (!ReadCreateRoomRequest(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const JoinRoomRequest& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteJoinRoomRequest(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, JoinRoomRequest& output)
    {
        if (type != TcpType::JoinRoomRequest || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        JoinRoomRequest value{};
        std::size_t offset = 0;
        if (!ReadJoinRoomRequest(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const RoomEnterResult& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteRoomEnterResult(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, RoomEnterResult& output)
    {
        if (type != TcpType::RoomEnterResult || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        RoomEnterResult value{};
        std::size_t offset = 0;
        if (!ReadRoomEnterResult(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const LeaveRoomRequest& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteLeaveRoomRequest(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, LeaveRoomRequest& output)
    {
        if (type != TcpType::LeaveRoomRequest || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        LeaveRoomRequest value{};
        std::size_t offset = 0;
        if (!ReadLeaveRoomRequest(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const LeaveRoomResult& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteLeaveRoomResult(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, LeaveRoomResult& output)
    {
        if (type != TcpType::LeaveRoomResult || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        LeaveRoomResult value{};
        std::size_t offset = 0;
        if (!ReadLeaveRoomResult(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const MapLoaded& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteMapLoaded(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, MapLoaded& output)
    {
        if (type != TcpType::MapLoaded || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        MapLoaded value{};
        std::size_t offset = 0;
        if (!ReadMapLoaded(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const ReadyChangeRequest& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteReadyChangeRequest(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, ReadyChangeRequest& output)
    {
        if (type != TcpType::ReadyChangeRequest || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        ReadyChangeRequest value{};
        std::size_t offset = 0;
        if (!ReadReadyChangeRequest(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const RoomState& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteRoomState(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, RoomState& output)
    {
        if (type != TcpType::RoomState || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        RoomState value{};
        std::size_t offset = 0;
        if (!ReadRoomState(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const GameStarted& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteGameStarted(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, GameStarted& output)
    {
        if (type != TcpType::GameStarted || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        GameStarted value{};
        std::size_t offset = 0;
        if (!ReadGameStarted(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const FireRequest& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteFireRequest(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, FireRequest& output)
    {
        if (type != TcpType::FireRequest || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        FireRequest value{};
        std::size_t offset = 0;
        if (!ReadFireRequest(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const ShotResolved& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteShotResolved(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, ShotResolved& output)
    {
        if (type != TcpType::ShotResolved || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        ShotResolved value{};
        std::size_t offset = 0;
        if (!ReadShotResolved(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const MatchTime& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteMatchTime(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, MatchTime& output)
    {
        if (type != TcpType::MatchTime || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        MatchTime value{};
        std::size_t offset = 0;
        if (!ReadMatchTime(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const GameResults& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteGameResults(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, GameResults& output)
    {
        if (type != TcpType::GameResults || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        GameResults value{};
        std::size_t offset = 0;
        if (!ReadGameResults(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const ReloadRequest& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteReloadRequest(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, ReloadRequest& output)
    {
        if (type != TcpType::ReloadRequest || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        ReloadRequest value{};
        std::size_t offset = 0;
        if (!ReadReloadRequest(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const WeaponState& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteWeaponState(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, WeaponState& output)
    {
        if (type != TcpType::WeaponState || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        WeaponState value{};
        std::size_t offset = 0;
        if (!ReadWeaponState(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const TimeProbeRequest& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteTimeProbeRequest(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, TimeProbeRequest& output)
    {
        if (type != TcpType::TimeProbeRequest || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        TimeProbeRequest value{};
        std::size_t offset = 0;
        if (!ReadTimeProbeRequest(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const TimeProbeResult& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteTimeProbeResult(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(TcpType type, const std::uint8_t* data,
        std::size_t size, TimeProbeResult& output)
    {
        if (type != TcpType::TimeProbeResult || size > MaxTcpPacketBytes - TcpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        TimeProbeResult value{};
        std::size_t offset = 0;
        if (!ReadTimeProbeResult(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const UdpRegisterRequest& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteUdpRegisterRequest(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(UdpType type, const std::uint8_t* data,
        std::size_t size, UdpRegisterRequest& output)
    {
        if (type != UdpType::RegisterRequest || size > MaxUdpPacketBytes - UdpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        UdpRegisterRequest value{};
        std::size_t offset = 0;
        if (!ReadUdpRegisterRequest(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const UdpRegisterResult& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteUdpRegisterResult(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(UdpType type, const std::uint8_t* data,
        std::size_t size, UdpRegisterResult& output)
    {
        if (type != UdpType::RegisterResult || size > MaxUdpPacketBytes - UdpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        UdpRegisterResult value{};
        std::size_t offset = 0;
        if (!ReadUdpRegisterResult(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const PlayerTransform& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WritePlayerTransform(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(UdpType type, const std::uint8_t* data,
        std::size_t size, PlayerTransform& output)
    {
        if (type != UdpType::PlayerTransform || size > MaxUdpPacketBytes - UdpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        PlayerTransform value{};
        std::size_t offset = 0;
        if (!ReadPlayerTransform(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const PlayerSnapshot& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WritePlayerSnapshot(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(UdpType type, const std::uint8_t* data,
        std::size_t size, PlayerSnapshot& output)
    {
        if (type != UdpType::PlayerSnapshot || size > MaxUdpPacketBytes - UdpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        PlayerSnapshot value{};
        std::size_t offset = 0;
        if (!ReadPlayerSnapshot(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::EncodeBody(const TargetSnapshot& value, std::vector<std::uint8_t>& bytes)
    {
        if (!ValidateBody(value))
        {
            return false;
        }
        WriteTargetSnapshot(bytes, value);
        return true;
    }

    bool PacketCodec::DecodeBody(UdpType type, const std::uint8_t* data,
        std::size_t size, TargetSnapshot& output)
    {
        if (type != UdpType::TargetSnapshot || size > MaxUdpPacketBytes - UdpHeaderBytes
            || (data == nullptr && size != 0))
        {
            return false;
        }
        TargetSnapshot value{};
        std::size_t offset = 0;
        if (!ReadTargetSnapshot(data, size, offset, value) || offset != size || !ValidateBody(value))
        {
            return false;
        }
        output = value;
        return true;
    }
    bool PacketCodec::ValidateHeader(const TcpHeader& header)
    {
        if (header.version != ProtocolVersion
            || header.size < TcpHeaderBytes || header.size > MaxTcpPacketBytes)
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
        case TcpType::ReloadRequest:
        case TcpType::WeaponState:
        case TcpType::TimeProbeRequest:
        case TcpType::TimeProbeResult:
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
        WriteTcpHeader(packet.bytes, header);
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            packet.bytes.push_back(bytes[i]);
        }
        output = packet;
        return true;
    }
    bool PacketCodec::ValidateHeader(const UdpHeader& header)
    {
        if (header.version != ProtocolVersion || header.magic != UdpMagic
            || header.size < UdpHeaderBytes || header.size > MaxUdpPacketBytes)
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
        if (!ValidateHeader(header) || bytes.size() > MaxUdpPacketBytes - UdpHeaderBytes
            || header.size != UdpHeaderBytes + bytes.size())
        {
            return false;
        }
        PacketData packet;
        packet.transport = PacketTransport::Udp;
        WriteUdpHeader(packet.bytes, header);
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            packet.bytes.push_back(bytes[i]);
        }
        output = packet;
        return true;
    }
    DecodeStatus PacketCodec::ReadTcpHeader(const std::uint8_t* data, std::size_t size,
        TcpType expected, TcpHeader& output)
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
        std::size_t offset = 0;
        if (!ReadTcpFields(data, size, offset, header) || !ValidateHeader(header) || header.type != expected)
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
        UdpType expected, UdpHeader& output)
    {
        if (data == nullptr || size < UdpHeaderBytes || size > MaxUdpPacketBytes)
        {
            return false;
        }
        UdpHeader header;
        std::size_t offset = 0;
        if (!ReadUdpFields(data, size, offset, header) || !ValidateHeader(header)
            || header.type != expected || header.size != size)
        {
            return false;
        }
        output = header;
        return true;
    }
}
