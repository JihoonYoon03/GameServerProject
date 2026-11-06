#include "pch.h"
#include "Room.h"
#include "Client.h"

#include <algorithm>
#include <cstring>
#include <limits>

void Room::SendTargetPositions(SOCKET udpSocket, const std::vector<Client>& clients,
	Net::ServerTimeMs nowMs)
{
	if (!targets_ || udpSocket == INVALID_SOCKET || clients.empty() || targetContext_.roomId == 0) return;

	const std::vector<Net::TargetMotionData> activeTargets = targets_->GetActiveTargets(nowMs);
	if (activeTargets.size() > Net::MaxTargets) return;

	const auto totalTargetCount = static_cast<std::uint16_t>(activeTargets.size());
	const auto chunkCount = static_cast<std::uint16_t>((std::max)(std::size_t{1},
		(activeTargets.size() + Net::MaxTargetsPerSnapshot - 1) / Net::MaxTargetsPerSnapshot));
	const Net::Sequence sequence = ++targetSnapshotSequence_;
	const auto serverTick = static_cast<std::uint32_t>(nowMs * Net::ServerTickRate / 1000);

	// 현재 UDP 헤더에는 버전 필드가 없으므로 선언된 필드만 직렬화한다.
	constexpr std::uint16_t HeaderBytes = 2 + 2 + 2 + Net::SessionTokenBytes + 4 + 4 + 4 + 4 + 4;
	constexpr std::uint16_t SnapshotMetadataBytes = 8 + 2 + 2 + 2 + 2;
	constexpr std::uint16_t TargetBytes = 4 + 3 * 4 + 3 * 4;
	static_assert(HeaderBytes + SnapshotMetadataBytes +
		Net::MaxTargetsPerSnapshot * TargetBytes <= Net::MaxUdpPacketBytes);

	for (std::uint16_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
	{
		const std::size_t offset = chunkIndex * Net::MaxTargetsPerSnapshot;
		const auto targetCount = static_cast<std::uint16_t>((std::min)(
			std::size_t{Net::MaxTargetsPerSnapshot}, activeTargets.size() - offset));
		const auto packetSize = static_cast<std::uint16_t>(
			HeaderBytes + SnapshotMetadataBytes + targetCount * TargetBytes);

		for (const auto& client : clients)
		{
			if (!client.UDPRegistered || client.roomId != targetContext_.roomId) continue;

			std::vector<std::uint8_t> bytes;
			bytes.reserve(packetSize);

			WriteUInt16(bytes, Net::UdpMagic);
			WriteUInt16(bytes, static_cast<std::uint16_t>(Net::UdpType::TargetSnapshot));
			WriteUInt16(bytes, packetSize);
			bytes.insert(bytes.end(), client.sessionToken.begin(), client.sessionToken.end());
			WriteUInt32(bytes, sequence);
			WriteUInt32(bytes, targetContext_.roomId);
			WriteUInt32(bytes, targetContext_.matchId);
			WriteUInt32(bytes, client.membershipId);
			WriteUInt32(bytes, serverTick);

			WriteUInt64(bytes, nowMs);
			WriteUInt16(bytes, totalTargetCount);
			WriteUInt16(bytes, chunkIndex);
			WriteUInt16(bytes, chunkCount);
			WriteUInt16(bytes, targetCount);
			for (std::size_t index = offset; index < offset + targetCount; ++index)
			{
				const auto& target = activeTargets[index];
				WriteUInt32(bytes, target.targetId);
				WriteFloat(bytes, target.position.x);
				WriteFloat(bytes, target.position.y);
				WriteFloat(bytes, target.position.z);
				WriteFloat(bytes, target.velocity.x);
				WriteFloat(bytes, target.velocity.y);
				WriteFloat(bytes, target.velocity.z);
			}

			// 이 방에 속하고 UDP 등록을 마친 참가자의 주소로 전송한다.
			const int result = sendto(udpSocket,
				reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()),
				0, reinterpret_cast<const sockaddr*>(&client.UDPAddress), sizeof(client.UDPAddress));
			if (result == SOCKET_ERROR) err_display("sendto()");
		}
	}
}

// 원본 값은 유지하고 전송 버퍼에 기록할 때 네트워크 바이트 순서로 변환한다.
void Room::WriteUInt16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
{
	const std::uint16_t networkValue = htons(value);
	const auto* data = reinterpret_cast<const std::uint8_t*>(&networkValue);
	bytes.insert(bytes.end(), data, data + sizeof(networkValue));
}

void Room::WriteUInt32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
	const std::uint32_t networkValue = htonl(value);
	const auto* data = reinterpret_cast<const std::uint8_t*>(&networkValue);
	bytes.insert(bytes.end(), data, data + sizeof(networkValue));
}

void Room::WriteUInt64(std::vector<std::uint8_t>& bytes, std::uint64_t value)
{
	// 상위 32비트와 하위 32비트를 차례로 변환해 기록한다.
	WriteUInt32(bytes, static_cast<std::uint32_t>(value >> 32));
	WriteUInt32(bytes, static_cast<std::uint32_t>(value));
}

void Room::WriteFloat(std::vector<std::uint8_t>& bytes, float value)
{
	static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
	// 실수는 비트 표현을 복사한 뒤 정수 변환 함수를 적용한다.
	std::uint32_t bits;
	std::memcpy(&bits, &value, sizeof(bits));
	WriteUInt32(bytes, bits);
}
