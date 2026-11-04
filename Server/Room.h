#pragma once
#include <winsock2.h>
#include "../Shared/NetworkMessages.h"
#include "Target.h"
#include <functional>
#include <utility>

// 경기 상태는 이 방의 스레드에서만 변경한다. 외부 요청은 AddCommand()로 전달한다.
class Client;

class Room
{
public:
	// 수신 대상은 모두 UDP 등록을 완료하고 이 방에 속한 참가자이다.
	struct TargetRecipient
	{
		Net::SessionToken sessionToken{};
		Net::MembershipId membershipId = 0;
	};
	using PacketSender = std::function<bool(const Net::SendRequest&)>;

	Room() = default;
	Room(Net::MatchContext context, Target& targets,
		std::vector<TargetRecipient> recipients, PacketSender sender)
		: targetContext_(context), targets_(&targets),
		  targetRecipients_(std::move(recipients)), sendPacket_(std::move(sender))
	{
	}
	void AssignPlayerAreas();
	void SetPlayerLoaded(const Net::SessionToken& player);
	void RemovePlayerFromRoom(const Net::SessionToken& player);
	void SetPlayerReady(const Net::SessionToken& player, bool ready);
	void SendRoomState();
	bool AreAllPlayersReady() const;
	void StartGameIfReady();
	void Tick();
	void StartRoomThread();
	void AddCommand(const Net::RoomCommand& command);
	void ProcessCommands();
	void StopRoom();
	void RecvPlayerInput(const Net::SessionToken& player,
		const Net::PlayerTransform& input);
	bool UpdatePlayerTransform(const Net::SessionToken& player,
		const Net::PlayerTransform& input);
	void CheckPlayerCollision(const Net::SessionToken& player);
	void SendTransformData();
	void RecvShoot(const Net::SessionToken& player, const Net::FireRequest& request);
	Net::ShotResolved JudgeShoot(const Net::SessionToken& player,
		const Net::FireRequest& request);
	void SendShootResult(const Net::ShotResolved& result);
	// 활성 표적 전체 목록을 동일 시점 기준으로 나눠 UDP 전송한다.
	void SendTargetPositions(SOCKET udpSocket, const std::vector<Client>& clients,
		Net::ServerTimeMs nowMs);
private:
	static void WriteUInt16(std::vector<std::uint8_t>& bytes, std::uint16_t value);
	static void WriteUInt32(std::vector<std::uint8_t>& bytes, std::uint32_t value);
	static void WriteUInt64(std::vector<std::uint8_t>& bytes, std::uint64_t value);
	static void WriteFloat(std::vector<std::uint8_t>& bytes, float value);

	Net::MatchContext targetContext_{};
	Target* targets_ = nullptr;
	std::vector<TargetRecipient> targetRecipients_;
	PacketSender sendPacket_;
	Net::Sequence targetSnapshotSequence_ = 0;
};

