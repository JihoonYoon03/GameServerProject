#include "NetworkFixtures.h"
#include "../Server/ClientPlayer.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
    int checks = 0;
    int tcpCases = 0;
    int udpCases = 0;
    Net::PacketData recordedPacket;

    bool RecordPacket(const Net::SendRequest& request)
    {
        recordedPacket = request.packet;
        return request.recipientToken == NetworkFixtures::Token();
    }

    void Check(bool ok, const char* message)
    {
        ++checks;
        if (!ok) throw std::runtime_error(message);
    }

    // 각 본문에 맞는 종류와 길이를 호출자가 직접 지정한다.
    template<class T>
    Net::TcpPacket<T> MakeTcp(const T& body, std::uint32_t size, std::uint32_t requestId = 0x12345678)
    {
        Net::TcpPacket<T> packet;
        packet.header.type = Net::PacketTraits<T>::type;
        packet.header.size = size;
        packet.header.requestId = requestId;
        packet.packetData = body;
        return packet;
    }

    template<class T>
    Net::UdpPacket<T> MakeUdp(const T& body, std::uint16_t size)
    {
        Net::UdpPacket<T> packet;
        packet.header = NetworkFixtures::Header();
        packet.header.type = Net::PacketTraits<T>::type;
        packet.header.size = size;
        packet.packetData = body;
        return packet;
    }

    template<class T>
    void RejectTcp(const std::vector<std::uint8_t>& bytes, const T& expectedBody)
    {
        Net::TcpPacket<T> output;
        output.packetData = expectedBody;
        output.header.requestId = 99;
        std::size_t consumed = 999;
        Check(Net::PacketCodec::DecodeTcp(bytes.data(), bytes.size(), output, consumed) == Net::DecodeStatus::Invalid
            && consumed == 0 && output.header.requestId == 99, "reject malformed TCP atomically");
    }

    template<class T>
    void RejectUdp(const std::vector<std::uint8_t>& bytes, const T& expectedBody)
    {
        Net::UdpPacket<T> output;
        output.packetData = expectedBody;
        output.header.sequence = 99;
        Check(!Net::PacketCodec::DecodeUdp(bytes.data(), bytes.size(), output)
            && output.header.sequence == 99, "reject malformed UDP atomically");
    }

    template<class T>
    void CheckTcp(const T& body, std::uint32_t length)
    {
        ++tcpCases;
        Net::TcpPacket<T> packet = MakeTcp(body, length);
        Net::PacketData encoded;
        Check(Net::PacketCodec::EncodeTcp(packet, encoded), "encode TCP case");
        Check(encoded.transport == Net::PacketTransport::Tcp && encoded.bytes.size() == length, "TCP exact wire length");
        unsigned type = static_cast<unsigned>(packet.header.type);
        Check(encoded.bytes[4] == 0 && encoded.bytes[5] == 5
            && encoded.bytes[6] == (type >> 8) && encoded.bytes[7] == (type & 255)
            && encoded.bytes[8] == 0x12 && encoded.bytes[9] == 0x34
            && encoded.bytes[10] == 0x56 && encoded.bytes[11] == 0x78, "TCP header golden offsets");

        Net::TcpPacket<T> decoded;
        std::size_t consumed = 999;
        Check(Net::PacketCodec::DecodeTcp(encoded.bytes.data(), encoded.bytes.size(), decoded, consumed)
            == Net::DecodeStatus::Complete, "decode TCP case");
        Check(consumed == length && decoded.header.type == packet.header.type, "TCP type and consumed length");
        Net::PacketData reencoded;
        Check(Net::PacketCodec::EncodeTcp(decoded, reencoded) && encoded.bytes == reencoded.bytes, "TCP round trip");
        T decodedBody;
        Check(Net::PacketCodec::DecodeBody(packet.header.type, encoded.bytes.data() + Net::TcpHeaderBytes,
            encoded.bytes.size() - Net::TcpHeaderBytes, decodedBody), "decode typed TCP body");
        Net::TcpPacket<T> bodyPacket = MakeTcp(decodedBody, length);
        Check(Net::PacketCodec::EncodeTcp(bodyPacket, reencoded) && reencoded.bytes == encoded.bytes, "TCP body round trip");

        for (std::size_t cut = 0; cut < encoded.bytes.size(); ++cut)
        {
            decoded.header.requestId = 99;
            Check(Net::PacketCodec::DecodeTcp(encoded.bytes.data(), cut, decoded, consumed) == Net::DecodeStatus::NeedMore
                && consumed == 0 && decoded.header.requestId == 99, "all TCP truncation points");
        }
        std::vector<std::uint8_t> combined = encoded.bytes;
        combined.insert(combined.end(), encoded.bytes.begin(), encoded.bytes.end());
        combined.insert(combined.end(), encoded.bytes.begin(), encoded.bytes.begin() + 6);
        std::size_t position = 0;
        for (int frame = 0; frame < 2; ++frame)
        {
            Check(Net::PacketCodec::DecodeTcp(combined.data() + position, combined.size() - position, decoded, consumed)
                == Net::DecodeStatus::Complete, "coalesced TCP frames");
            position += consumed;
        }
        Check(position == encoded.bytes.size() * 2, "coalesced TCP offset");
        Check(Net::PacketCodec::DecodeTcp(combined.data() + position, combined.size() - position, decoded, consumed)
            == Net::DecodeStatus::NeedMore, "retain partial next TCP frame");

        // 헤더 종류나 길이가 본문과 다르면 송신 전에 거절해야 한다.
        Net::PacketData unchanged{Net::PacketTransport::Udp, {9, 8, 7}};
        packet.header.type = Net::TcpType::TimeProbeRequest;
        if (packet.header.type == Net::PacketTraits<T>::type) packet.header.type = Net::TcpType::RoomListRequest;
        Check(!Net::PacketCodec::EncodeTcp(packet, unchanged) && unchanged.bytes == std::vector<std::uint8_t>({9, 8, 7})
            && unchanged.transport == Net::PacketTransport::Udp, "reject mismatched TCP type");
        packet.header.type = Net::PacketTraits<T>::type;
        ++packet.header.size;
        Check(!Net::PacketCodec::EncodeTcp(packet, unchanged), "reject mismatched TCP size");
        std::vector<std::uint8_t> extra = encoded.bytes;
        extra.push_back(0);
        std::uint32_t extraLength = static_cast<std::uint32_t>(extra.size());
        extra[0] = static_cast<std::uint8_t>(extraLength >> 24);
        extra[1] = static_cast<std::uint8_t>(extraLength >> 16);
        extra[2] = static_cast<std::uint8_t>(extraLength >> 8);
        extra[3] = static_cast<std::uint8_t>(extraLength);
        RejectTcp(extra, body);
    }

    template<class T>
    void CheckUdp(const T& body, std::uint16_t length)
    {
        ++udpCases;
        Net::UdpPacket<T> packet = MakeUdp(body, length);
        Net::PacketData encoded;
        Check(Net::PacketCodec::EncodeUdp(packet, encoded), "encode UDP case");
        Check(encoded.transport == Net::PacketTransport::Udp && encoded.bytes.size() == length, "UDP exact wire length");
        const std::vector<std::uint8_t>& bytes = encoded.bytes;
        Check(bytes[0] == 0x43 && bytes[1] == 0x47 && bytes[2] == 0 && bytes[3] == 5
            && bytes[8] == 1 && bytes[23] == 16 && bytes[27] == 42
            && bytes[31] == 7 && bytes[35] == 3 && bytes[39] == 2 && bytes[43] == 30, "44 byte UDP header golden offsets");
        Net::UdpPacket<T> decoded;
        Check(Net::PacketCodec::DecodeUdp(bytes.data(), bytes.size(), decoded), "decode UDP case");
        Check(decoded.header.type == packet.header.type, "UDP message type");
        Net::PacketData reencoded;
        Check(Net::PacketCodec::EncodeUdp(decoded, reencoded) && bytes == reencoded.bytes, "UDP round trip");
        T decodedBody;
        Check(Net::PacketCodec::DecodeBody(packet.header.type, bytes.data() + Net::UdpHeaderBytes,
            bytes.size() - Net::UdpHeaderBytes, decodedBody), "decode typed UDP body");
        Net::UdpPacket<T> bodyPacket = MakeUdp(decodedBody, length);
        Check(Net::PacketCodec::EncodeUdp(bodyPacket, reencoded) && bytes == reencoded.bytes, "UDP body round trip");
        for (std::size_t cut = 0; cut < bytes.size(); ++cut)
            RejectUdp(std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + cut), body);
        std::vector<std::uint8_t> extra = bytes;
        extra.push_back(0);
        RejectUdp(extra, body);
        extra[6] = static_cast<std::uint8_t>(extra.size() >> 8);
        extra[7] = static_cast<std::uint8_t>(extra.size());
        RejectUdp(extra, body);
        Net::PacketData unchanged{Net::PacketTransport::Tcp, {9, 8, 7}};
        packet.header.type = Net::UdpType::RegisterRequest;
        if (packet.header.type == Net::PacketTraits<T>::type) packet.header.type = Net::UdpType::RegisterResult;
        Check(!Net::PacketCodec::EncodeUdp(packet, unchanged) && unchanged.bytes == std::vector<std::uint8_t>({9, 8, 7})
            && unchanged.transport == Net::PacketTransport::Tcp, "reject mismatched UDP type");
        packet.header.type = Net::PacketTraits<T>::type;
        ++packet.header.size;
        Check(!Net::PacketCodec::EncodeUdp(packet, unchanged), "reject mismatched UDP size");
    }

    void TestPrimitives()
    {
        std::vector<std::uint8_t> bytes;
        Net::WriteU8(bytes, 0xAB);
        Net::WriteU16(bytes, 0x1234);
        Net::WriteU32(bytes, 0x89ABCDEF);
        Net::WriteU64(bytes, 0x0123456789ABCDEFull);
        Net::WriteF32(bytes, 1.0f);
        Net::WriteF32(bytes, -2.5f);
        Net::WriteF32(bytes, -0.0f);
        const std::vector<std::uint8_t> expected{0xAB, 0x12, 0x34, 0x89, 0xAB, 0xCD, 0xEF,
            0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
            0x3F, 0x80, 0, 0, 0xC0, 0x20, 0, 0, 0x80, 0, 0, 0};
        Check(bytes == expected, "big endian integer and float golden bytes");
        std::size_t offset = 0;
        std::uint8_t u8 = 0;
        std::uint16_t u16 = 0;
        std::uint32_t u32 = 0;
        std::uint64_t u64 = 0;
        float value = 0;
        Check(Net::ReadU8(bytes.data(), bytes.size(), offset, u8) && u8 == 0xAB, "read u8");
        Check(Net::ReadU16(bytes.data(), bytes.size(), offset, u16) && u16 == 0x1234, "read u16");
        Check(Net::ReadU32(bytes.data(), bytes.size(), offset, u32) && u32 == 0x89ABCDEF, "read u32");
        Check(Net::ReadU64(bytes.data(), bytes.size(), offset, u64) && u64 == 0x0123456789ABCDEFull, "read u64");
        Check(Net::ReadF32(bytes.data(), bytes.size(), offset, value) && value == 1.0f, "read f32 positive");
        Check(Net::ReadF32(bytes.data(), bytes.size(), offset, value) && value == -2.5f, "read f32 negative");
        Check(Net::ReadF32(bytes.data(), bytes.size(), offset, value), "read f32 signed zero");
        std::memcpy(&u32, &value, 4);
        Check(u32 == 0x80000000, "float bit preservation");
        std::size_t end = offset;
        u64 = 17;
        Check(!Net::ReadU64(bytes.data(), bytes.size(), offset, u64) && offset == end && u64 == 17, "read failure atomicity");
        offset = std::numeric_limits<std::size_t>::max();
        Check(!Net::ReadU64(bytes.data(), bytes.size(), offset, u64), "offset overflow guard");
        offset = 0;
        Check(!Net::ReadU8(nullptr, 1, offset, u8), "null read guard");
        const std::uint32_t patterns[]{0x00000001u, 0x7F800000u, 0xFF800000u, 0x7FC01234u};
        for (std::uint32_t bits : patterns)
        {
            std::memcpy(&value, &bits, 4);
            bytes.clear();
            Net::WriteF32(bytes, value);
            offset = 0;
            Check(Net::ReadF32(bytes.data(), bytes.size(), offset, value), "read IEEE bits");
            std::memcpy(&u32, &value, 4);
            Check(u32 == bits, "IEEE bit round trip");
        }
    }

    void TestTcp()
    {
        using namespace Net;
        RoomStateData room = NetworkFixtures::RoomState();
        SessionToken token = NetworkFixtures::Token();
        FireRequest fire = std::get<FireRequest>(NetworkFixtures::FireCommand().body);
        CheckTcp(ConnectResult{ErrorCode::Ok, token, 7778, 123456, 30}, 42);
        CheckTcp(RequestRejected{ErrorCode::StaleMatch, {7, 3}}, 22);
        CheckTcp(RoomListRequest{123}, 16);
        CheckTcp(RoomListResult{99, {{7, 4, 4, GameState::Preparing}, {8, 2, 4, GameState::Playing}}}, 32);
        CheckTcp(CreateRoomRequest{}, 12);
        CheckTcp(JoinRoomRequest{7}, 16);
        CheckTcp(RoomEnterResult{ErrorCode::Ok, 2, room}, 764);
        CheckTcp(LeaveRoomRequest{7}, 16);
        CheckTcp(LeaveRoomResult{ErrorCode::Ok, 7}, 18);
        CheckTcp(MapLoaded{7}, 16);
        CheckTcp(ReadyChangeRequest{7, 1}, 17);
        CheckTcp(RoomState{room}, 758);
        CheckTcp(GameStarted{room.version, 1000, 2000, 122000, room.rules, room.players}, 422);
        CheckTcp(fire, 108);
        CheckTcp(ShotResolved{room.version, token, 11, ShotOutcome::HitTarget, 8, {12, 14}}, 57);
        CheckTcp(MatchTime{{7, 3}, 5000, 122000}, 36);
        CheckTcp(GameResults{room, 122000, MatchEndReason::TimeLimit,
            {{token, {12, 14}, 1}, {token, {10, 12}, 2}, {token, {5, 9}, 3}, {token, {0, 2}, 4}}}, 869);
        CheckTcp(ReloadRequest{{7, 3}, 2}, 24);
        CheckTcp(WeaponState{room.version, token, 3, 1, 2100, 4000}, 58);
        CheckTcp(TimeProbeRequest{0x0102030405060708ull}, 20);
        CheckTcp(TimeProbeResult{1000, 1010, 1012}, 36);
        Check(tcpCases == 21, "21 TCP messages tested");

        RoomEnterResult denied;
        denied.result = ErrorCode::RoomFull;
        denied.membershipId = 17;
        denied.room.capacity = 255; // 입장 실패 응답에서는 이 필드를 보내지 않는다.
        TcpPacket<RoomEnterResult> deniedPacket = MakeTcp(denied, 14, 7);
        PacketData failure;
        Check(PacketCodec::EncodeTcp(deniedPacket, failure) && failure.bytes.size() == 14, "failed room enter omits room and membership");
        TcpPacket<RoomEnterResult> decoded;
        std::size_t consumed = 0;
        Check(PacketCodec::DecodeTcp(failure.bytes.data(), failure.bytes.size(), decoded, consumed) == DecodeStatus::Complete, "decode failed room enter");
        Check(decoded.packetData.membershipId == 0 && decoded.packetData.room.capacity == MaxPlayers, "omitted fields use defaults");

        PacketData fireWire = NetworkFixtures::FirePacket();
        const std::vector<std::uint8_t> expectedPrefix{0,0,0,108, 0,5,3,2, 0,0,0,17,
            0,0,0,7, 0,0,0,3, 0,0,0,2, 0,0,0,11, 0,0,0,42, 0x3F,0x80,0,0};
        Check(std::equal(expectedPrefix.begin(), expectedPrefix.end(), fireWire.bytes.begin()), "fire command golden prefix");
        Check(fireWire.bytes[100] == 0x3F && fireWire.bytes[101] == 0x80, "fire origin golden offset");
    }

    void TestUdp()
    {
        Net::RoomStateData room = NetworkFixtures::RoomState();
        Net::PlayerSnapshot players;
        players.serverTimeMs = 123456;
        players.requiredRevision = 9;
        players.playerCount = Net::MaxPlayers;
        for (std::size_t i = 0; i < Net::MaxPlayers; ++i)
            players.players[i] = {room.players[i].m_sessionToken, room.players[i].worldTransform, static_cast<Net::Sequence>(40 + i)};
        CheckUdp(Net::UdpRegisterRequest{}, 44);
        CheckUdp(Net::UdpRegisterResult{Net::ErrorCode::Ok, 42}, 50);
        CheckUdp(Net::PlayerTransform{42, room.players[3].worldTransform}, 112);
        CheckUdp(players, 393);
        CheckUdp(NetworkFixtures::Targets(), 1180);
        Check(udpCases == 5, "5 UDP messages tested");

        const std::uint16_t totals[]{0, 40, 90};
        for (std::uint16_t total : totals)
        {
            std::uint16_t chunks = total == 90 ? 3 : 1;
            std::size_t collected = 0;
            for (std::uint16_t index = 0; index < chunks; ++index)
            {
                Net::PacketData encoded = NetworkFixtures::TargetPacket(total, index);
                std::size_t count = total == 0 ? 0 : (total == 90 && index == 2 ? 10 : 40);
                Check(encoded.bytes.size() == 60 + 28 * count, "target empty and 40/40/10 exact lengths");
                Check(encoded.bytes[56] == 0 && encoded.bytes[57] == chunks
                    && encoded.bytes[58] == 0 && encoded.bytes[59] == count, "target chunk golden metadata");
                Net::UdpPacket<Net::TargetSnapshot> decoded;
                Check(Net::PacketCodec::DecodeUdp(encoded.bytes.data(), encoded.bytes.size(), decoded), "decode target chunks");
                collected += decoded.packetData.targetCount;
                if (count != 0)
                    Check(encoded.bytes[63] == index * 40 + 1 && encoded.bytes[64] == 0xBF
                        && encoded.bytes[65] == 0xA0, "target id and position golden offsets");
            }
            Check(collected == total, "target chunks account for total");
        }
    }

    template<class T>
    void RejectTcpBody(const T& body, std::uint32_t length)
    {
        Check(!Net::PacketCodec::ValidateBody(body), "reject invalid TCP body");
        Net::TcpPacket<T> packet = MakeTcp(body, length);
        Net::PacketData output{Net::PacketTransport::Udp, {9, 8, 7}};
        Check(!Net::PacketCodec::EncodeTcp(packet, output) && output.bytes == std::vector<std::uint8_t>({9, 8, 7}), "invalid body preserves output");
    }

    void TestInvalid()
    {
        Net::ReadyChangeRequest ready{7, 1};
        Net::TcpPacket<Net::ReadyChangeRequest> packet = MakeTcp(ready, 17, 1);
        Net::PacketData valid;
        Check(Net::PacketCodec::EncodeTcp(packet, valid), "invalid TCP fixture");
        const unsigned tcpOffsets[]{4, 5, 6, 7};
        for (unsigned offset : tcpOffsets)
        {
            std::vector<std::uint8_t> bytes = valid.bytes;
            bytes[offset] = 0xFF;
            RejectTcp(bytes, ready);
        }
        std::vector<std::uint8_t> bytes = valid.bytes;
        bytes[3] = 11; RejectTcp(bytes, ready);
        bytes = valid.bytes; bytes[0] = 1; RejectTcp(bytes, ready);
        bytes = valid.bytes; bytes[16] = 2; RejectTcp(bytes, ready);
        Net::PacketData rooms;
        Check(Net::PacketCodec::EncodeTcp(MakeTcp(Net::RoomListResult{}, 18), rooms), "room count fixture");
        bytes = rooms.bytes; bytes[16] = 0xFF; bytes[17] = 0xFF; RejectTcp(bytes, Net::RoomListResult{});
        RejectTcp(valid.bytes, Net::JoinRoomRequest{});
        Net::TcpHeader tcp;
        tcp.size = 65536;
        Check(Net::PacketCodec::ValidateHeader(tcp), "TCP maximum length accepted");
        tcp.size = 65537;
        Check(!Net::PacketCodec::ValidateHeader(tcp), "TCP over maximum rejected");
        Net::UdpHeader udp;
        udp.size = 1200;
        Check(Net::PacketCodec::ValidateHeader(udp), "UDP maximum length accepted");
        udp.size = 1201;
        Check(!Net::PacketCodec::ValidateHeader(udp), "UDP over maximum rejected");

        Net::PacketData datagram = NetworkFixtures::TargetPacket();
        const unsigned udpOffsets[]{0, 1, 2, 3, 4, 5, 6, 7, 52, 54, 56, 58};
        for (unsigned offset : udpOffsets)
        {
            bytes = datagram.bytes;
            bytes[offset] = 0xFF;
            RejectUdp(bytes, Net::TargetSnapshot{});
        }
        bytes = datagram.bytes; bytes[56] = 0; bytes[57] = 2; RejectUdp(bytes, Net::TargetSnapshot{});
        bytes.assign(1201, 0); RejectUdp(bytes, Net::TargetSnapshot{});
        RejectUdp(datagram.bytes, Net::UdpRegisterResult{});

        RejectTcpBody(Net::ReadyChangeRequest{7, 2}, 17);
        Net::RoomStateData room = NetworkFixtures::RoomState();
        room.players[0].ready = static_cast<Net::ReadyState>(255); RejectTcpBody(Net::RoomState{room}, 758);
        room = NetworkFixtures::RoomState(); room.players[0].mapLoaded = 2; RejectTcpBody(Net::RoomState{room}, 758);
        room = NetworkFixtures::RoomState(); room.players.push_back(room.players[0]); RejectTcpBody(Net::RoomState{room}, 849);
        room = NetworkFixtures::RoomState(); room.areas.push_back(room.areas[0]); RejectTcpBody(Net::RoomState{room}, 847);
        room = NetworkFixtures::RoomState(); room.state = static_cast<Net::GameState>(255); RejectTcpBody(Net::RoomState{room}, 758);
        Net::RoomListResult tooMany; tooMany.rooms.resize(65); RejectTcpBody(tooMany, 473);
        Net::GameResults results; results.players.resize(5); RejectTcpBody(results, 174);
        Net::GameStarted started; started.players.resize(5); RejectTcpBody(started, 513);
        RejectTcpBody(Net::ConnectResult{static_cast<Net::ErrorCode>(65535), {}, 0, 0, 30}, 42);
        RejectTcpBody(Net::WeaponState{{}, {}, 7, 0, 0, 0}, 58);
        RejectTcpBody(Net::WeaponState{{}, {}, 6, 2, 0, 0}, 58);
        Net::ShotResolved shot; shot.outcome = static_cast<Net::ShotOutcome>(255); RejectTcpBody(shot, 57);
        results = {}; results.reason = static_cast<Net::MatchEndReason>(255); RejectTcpBody(results, 49);
        Net::PlayerSnapshot players; players.playerCount = 5;
        Check(!Net::PacketCodec::ValidateBody(players), "reject player count above four");
        Net::TargetSnapshot targets = NetworkFixtures::Targets(); targets.targetCount = 41;
        Check(!Net::PacketCodec::ValidateBody(targets), "reject target count above forty");
        targets = NetworkFixtures::Targets(); targets.totalTargetCount = 91;
        Check(!Net::PacketCodec::ValidateBody(targets), "reject target total above ninety");
        Net::JoinRoomRequest body{99};
        Check(!Net::PacketCodec::DecodeBody(static_cast<Net::TcpType>(65535), nullptr, 0, body)
            && body.roomId == 99, "unknown body type preserves output");
        Net::TcpPacket<Net::ReadyChangeRequest> output;
        std::size_t consumed = 9;
        Check(Net::PacketCodec::DecodeTcp(nullptr, 0, output, consumed) == Net::DecodeStatus::NeedMore && consumed == 0, "empty TCP stream");
        Check(Net::PacketCodec::DecodeTcp(nullptr, 12, output, consumed) == Net::DecodeStatus::Invalid, "null TCP buffer");
        Net::UdpPacket<Net::TargetSnapshot> udpOutput;
        Check(!Net::PacketCodec::DecodeUdp(nullptr, 44, udpOutput), "null UDP buffer");
    }

    void TestWireOnlyValues()
    {
        // 경기·수학 담당이 판단할 값은 코덱이 바꾸거나 거절하지 않는다.
        Net::RoomStateData room = NetworkFixtures::RoomState();
        room.capacity = 2;
        room.players.resize(2);
        room.rules.durationMs = 0;
        room.rules.pointsPerHit = 0;
        room.areas[0].areaId = 255;
        room.players[0].areaId = 200;
        room.areas[0].movementBounds.min.x = 10.0f;
        room.players[0].worldTransform.elements[0] = std::numeric_limits<float>::quiet_NaN();
        Net::TcpPacket<Net::RoomState> packet = MakeTcp(Net::RoomState{room}, 576);
        Net::PacketData encoded;
        Check(Net::PacketCodec::EncodeTcp(packet, encoded), "codec accepts values reserved for gameplay validation");
        Net::TcpPacket<Net::RoomState> decoded;
        std::size_t consumed = 0;
        Check(Net::PacketCodec::DecodeTcp(encoded.bytes.data(), encoded.bytes.size(), decoded, consumed) == Net::DecodeStatus::Complete,
            "wire values decode without gameplay policy");
        Check(decoded.packetData.room.rules.durationMs == 0 && decoded.packetData.room.rules.pointsPerHit == 0
            && decoded.packetData.room.areas[0].areaId == 255 && decoded.packetData.room.players[0].areaId == 200
            && decoded.packetData.room.areas[0].movementBounds.min.x == 10.0f, "wire values preserved");
        Net::PacketData reencoded;
        Check(Net::PacketCodec::EncodeTcp(decoded, reencoded) && encoded.bytes == reencoded.bytes, "NaN bits preserved in a complete packet");
        Net::GameStarted started;
        started.startsAtMs = 2000;
        started.endsAtMs = 1000;
        started.rules.durationMs = 0;
        started.rules.pointsPerHit = 0;
        Check(Net::PacketCodec::EncodeTcp(MakeTcp(started, 58), encoded), "codec accepts reversed gameplay timestamps");
        Net::GameResults results;
        results.players.resize(4);
        Check(Net::PacketCodec::EncodeTcp(MakeTcp(results, 149), encoded), "codec accepts rank zero for four results");
        Net::TcpPacket<Net::GameResults> resultPacket;
        Check(Net::PacketCodec::DecodeTcp(encoded.bytes.data(), encoded.bytes.size(), resultPacket, consumed) == Net::DecodeStatus::Complete
            && resultPacket.packetData.players.size() == 4 && resultPacket.packetData.players[0].rank == 0, "rank zero round trip");
        Net::TargetSnapshot targets = NetworkFixtures::Targets(40);
        targets.targets[0].position.x = std::numeric_limits<float>::infinity();
        Check(Net::PacketCodec::EncodeUdp(MakeUdp(targets, 1180), encoded), "codec accepts infinity bits");
        Net::UdpPacket<Net::TargetSnapshot> targetPacket;
        Check(Net::PacketCodec::DecodeUdp(encoded.bytes.data(), encoded.bytes.size(), targetPacket), "infinity bits decode");
        Check(Net::PacketCodec::EncodeUdp(targetPacket, reencoded) && encoded.bytes == reencoded.bytes, "infinity bits round trip");
        Net::RoomState emptyRoom;
        emptyRoom.room.capacity = 0;
        Check(Net::PacketCodec::EncodeTcp(MakeTcp(emptyRoom, 38), encoded), "codec accepts zero capacity as a wire value");
        Check(Net::PacketCodec::EncodeTcp(MakeTcp(Net::RequestRejected{Net::ErrorCode::Ok, {}}, 22), encoded),
            "codec leaves rejection policy to the request handler");
    }

    void TestDefaults()
    {
        ClientPlayer player;
        Check(player.state.areaId == Net::UnassignedArea && player.state.ready == Net::ReadyState::Connecting
            && player.lastProcessedInput == 0 && player.lastProcessedShot == 0
            && !player.hasProcessedInput && player.lastMovementBudgetMs == 0, "ClientPlayer initial state");
        Check(player.weapon.ammo == 6 && !player.weapon.reloading && player.weapon.nextFireAtMs == 0
            && player.weapon.reloadEndsAtMs == 0 && player.lastProcessedTransform.elements[0] == 1.0f, "weapon and transform defaults");
        Net::RoomCommand command = NetworkFixtures::FireCommand();
        command.body = Net::ReloadRequest{command.context, command.membershipId};
        Check(std::holds_alternative<Net::ReloadRequest>(command.body), "reload command alternative");
        Net::PacketSender sender = RecordPacket;
        Net::PacketData wire = NetworkFixtures::FirePacket();
        Check(sender({NetworkFixtures::Token(), wire}) && recordedPacket.bytes == wire.bytes, "recording sender contract");
    }
}

int main()
{
    try
    {
        TestPrimitives();
        TestTcp();
        TestUdp();
        TestInvalid();
        TestWireOnlyValues();
        TestDefaults();
        std::cout << "PASS: 21 TCP / 5 UDP messages, " << checks << " checks\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
