#pragma once
#include "../Shared/PacketCodec.h"
#include <stdexcept>

// 소켓·시각·그래픽·게임 진행 코드 없이 코덱을 검사하는 고정 입력이다.
namespace NetworkFixtures
{
    inline Net::SessionToken Token(std::uint8_t first = 1)
    {
        Net::SessionToken token{};
        for (std::size_t i = 0; i < token.size(); ++i) token[i] = static_cast<std::uint8_t>(first + i);
        return token;
    }
    inline Net::RoomStateData RoomState()
    {
        Net::RoomStateData room;
        room.version = {{7, 3}, 9};
        room.state = Net::GameState::Preparing;
        for (std::uint8_t i = 0; i < Net::MaxPlayers; ++i)
        {
            Net::AreaData area;
            area.areaId = i;
            area.movementBounds = {{-5.0f, 0.0f, -5.0f}, {5.0f, 3.0f, 5.0f}};
            room.areas.push_back(area);
            Net::PlayerData player;
            player.sessionToken = Token(static_cast<std::uint8_t>(i * 16 + 1));
            player.areaId = i;
            player.ready = Net::ReadyState::Ready;
            player.mapLoaded = 1;
            player.worldTransform.elements[12] = static_cast<float>(i);
            player.stats = {i * 2u, i * 3u};
            room.players.push_back(player);
        }
        return room;
    }
    inline Net::TargetSnapshot Targets(std::uint16_t total = 90, std::uint16_t index = 0)
    {
        Net::TargetSnapshot frame;
        frame.serverTimeMs = 123456;
        frame.totalTargetCount = total;
        frame.chunkIndex = index;
        frame.chunkCount = total == 0 ? 1 : (total + Net::MaxTargetsPerSnapshot - 1) / Net::MaxTargetsPerSnapshot;
        if (total > Net::MaxTargets || index >= frame.chunkCount)
            throw std::out_of_range("Invalid target fixture chunk");
        const auto remaining = total - index * Net::MaxTargetsPerSnapshot;
        frame.targetCount = static_cast<std::uint16_t>(remaining < Net::MaxTargetsPerSnapshot ? remaining : Net::MaxTargetsPerSnapshot);
        for (std::uint16_t i = 0; i < frame.targetCount; ++i)
            frame.targets[i] = {static_cast<Net::TargetId>(index * Net::MaxTargetsPerSnapshot + i + 1),
                {-1.25f, 1.0f, -2.5f}, {0.5f, 0.0f, -0.25f}};
        return frame;
    }
    inline Net::UdpHeader Header()
    {
        Net::UdpHeader header;
        header.sessionToken = Token();
        header.sequence = 42;
        header.context = {7, 3};
        header.membershipId = 2;
        header.serverTick = 30;
        return header;
    }
    inline Net::RoomCommand FireCommand()
    {
        Net::RoomCommand command;
        command.sessionToken = Token();
        command.requestId = 17;
        command.context = {7, 3};
        command.membershipId = 2;
        command.receivedAtMs = 123456;
        Net::FireRequest fire;
        fire.context = command.context;
        fire.membershipId = command.membershipId;
        fire.shotId = 11;
        fire.inputSequence = 42;
        fire.origin = {0.0f, 1.0f, 0.0f};
        command.body = fire;
        return command;
    }
    inline Net::PacketData FirePacket()
    {
        const auto command = FireCommand();
        Net::TcpPacket<Net::FireRequest> packet;
        packet.header.type = Net::TcpType::FireRequest;
        packet.header.size = 108;
        packet.header.requestId = command.requestId;
        packet.packetData = std::get<Net::FireRequest>(command.body);
        Net::PacketData output;
        if (!Net::PacketCodec::EncodeTcp(packet, output))
            throw std::logic_error("Invalid fixed fire fixture");
        return output;
    }
    inline Net::PacketData TargetPacket(std::uint16_t total = 90, std::uint16_t index = 0)
    {
        Net::UdpPacket<Net::TargetSnapshot> packet;
        packet.header = Header();
        packet.header.type = Net::UdpType::TargetSnapshot;
        packet.packetData = Targets(total, index);
        packet.header.size = static_cast<std::uint16_t>(60 + 28 * packet.packetData.targetCount);
        Net::PacketData output;
        if (!Net::PacketCodec::EncodeUdp(packet, output))
            throw std::logic_error("Invalid fixed target fixture");
        return output;
    }
}
