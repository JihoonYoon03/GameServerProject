// 운영체제·그래픽·행렬 라이브러리와 게임의 미리 컴파일된 헤더 없이 컴파일되어야 한다.
#include "../AimLab/Shared/NetworkMessages.h"
#include <type_traits>
#include <limits>

static_assert(std::tuple_size<Net::SessionToken>::value == 16);
static_assert(std::tuple_size<decltype(Net::Mat4Data::elements)>::value == 16);
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

static_assert(std::is_same_v<decltype(Net::PlayerData::sessionToken), Net::SessionToken>);
static_assert(std::is_same_v<decltype(Net::FireRequest::shooterTransform), Net::Mat4Data>);
static_assert(std::is_same_v<decltype(Net::ShotResolved::shooterStats), Net::PlayerStats>);
