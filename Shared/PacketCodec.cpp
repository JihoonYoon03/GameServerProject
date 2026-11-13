#include "PacketCodec.h"

#pragma comment(lib, "Ws2_32.lib")

namespace Net
{
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
        case TcpType::CreateRoomRequest:
        case TcpType::RoomEnterResult:
        case TcpType::LeaveRoomRequest:
        case TcpType::LeaveRoomResult:
        case TcpType::MapLoaded:
        case TcpType::RoomState:
        case TcpType::FireRequest:
        case TcpType::ShotResolved:
        case TcpType::GameStarted:
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
        PacketDetail::PacketStream io(packet.bytes);
        if (!PacketDetail::Serialize(io, candidate))
            return false;
        packet.bytes.insert(packet.bytes.end(), bytes.begin(), bytes.end());
        output = std::move(packet);
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
        PacketDetail::PacketStream io(packet.bytes);
        if (!PacketDetail::Serialize(io, candidate))
            return false;
        packet.bytes.insert(packet.bytes.end(), bytes.begin(), bytes.end());
        output = std::move(packet);
        return true;
    }
    DecodeStatus PacketCodec::ReadTcpHeader(const std::uint8_t* data, std::size_t size, TcpHeader& output)
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
        PacketDetail::PacketStream io(data, size);
        if (!PacketDetail::Serialize(io, header) || !ValidateHeader(header))
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
        PacketDetail::PacketStream io(data, size);
        if (!PacketDetail::Serialize(io, header) || !ValidateHeader(header))
        {
            return false;
        }
        output = header;
        return true;
    }
}
