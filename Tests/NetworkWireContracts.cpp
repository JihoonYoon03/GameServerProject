// 운영체제·그래픽·행렬 라이브러리와 게임의 미리 컴파일된 헤더 없이 컴파일되어야 한다.
#include "../Shared/NetworkMessages.h"
#include "../Shared/PacketCodec.h"
#include <limits>

static_assert(Net::SessionToken{}.size() == 16);
static_assert(Net::Mat4Data{}.elements.size() == 16);
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
    "Wire floats require IEEE-754 binary32");

// 아래 크기는 필드별 직렬화 길이의 합이며, 메모리에 배치된 구조체 크기가 아니다.
static_assert(Net::TcpHeaderBytes == 4 + 2 + 2 + 4);
static_assert(Net::UdpHeaderBytes == 2 + 2 + 2 + 2 + 16 + 4 + 8 + 4 + 4);
static_assert(Net::UdpHeaderBytes + 4 + 16 * 4 == 112);
static_assert(Net::UdpHeaderBytes + 8 + 4 + 1
    + Net::MaxPlayers * (16 + 64 + 4) == 393);
static_assert(Net::UdpHeaderBytes + 8 + 2 + 2 + 2 + 2
    + Net::MaxTargetsPerSnapshot * (4 + 12 + 12) == 1180);
static_assert(1180 <= Net::MaxUdpPacketBytes);
static_assert(Net::MaxTargetSnapshotChunks == 3);

static_assert(Net::TcpPacket<Net::FireRequest>{}.packetData.shooterTransform.elements.size() == 16);
static_assert(Net::UdpPacket<Net::TargetSnapshot>{}.packetData.targets.size() == 40);
static_assert(Net::PacketTraits<Net::ReloadRequest>::type == Net::TcpType::ReloadRequest);
static_assert(Net::PacketTraits<Net::WeaponState>::type == Net::TcpType::WeaponState);
static_assert(Net::PacketTraits<Net::UdpRegisterRequest>::type == Net::UdpType::RegisterRequest);
static_assert(Net::PacketTraits<Net::TargetSnapshot>::transport == Net::PacketTransport::Udp);
static_assert(static_cast<std::uint16_t>(Net::TcpType::ReloadRequest) == 0x0307);
static_assert(static_cast<std::uint16_t>(Net::TcpType::WeaponState) == 0x0308);
