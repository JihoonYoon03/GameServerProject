#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "../Shared/NetworkMessages.h"
#include <memory>
#include <queue>
#include <unordered_map>

class Room;

class RoomManager
{
public:
	explicit RoomManager(Net::PacketSender sender = {});
	~RoomManager();
	RoomManager(const RoomManager&) = delete;
	RoomManager& operator=(const RoomManager&) = delete;

	void SendRoomList(const Net::SessionToken& recipient,
		const Net::RoomListRequest& request);
	// 방 목록과 다인 입장은 11/18 확장 일정에서 구현한다.
	void CreateGameRoom(const Net::SessionToken& creator);
	// 운영 1인 방의 입장 정보는 서버 메인이 확인해서 전달한다.
	Net::RoomId CreateGameRoom(const Net::SessionToken& creator,
		Net::MembershipId membershipId, bool UDPRegistered, std::uint32_t requestId = 0);
	void JoinGameRoom(const Net::SessionToken& player,
		const Net::JoinRoomRequest& request);
	Net::ErrorCode CheckRoomJoin(Net::RoomId roomId) const;
	bool AddCommand(const Net::RoomCommand& command);
	bool HasRoom(Net::RoomId roomId) const;
	// 요청 전달 중지와 방 스레드 종료를 확인한 뒤 방 객체를 삭제한다.
	void DeleteRoom(Net::RoomId roomId);
	void StopRooms();
	void UpdateRooms();

private:
	bool StartRoomThread(Net::RoomId roomId);
	std::unordered_map<Net::RoomId, std::unique_ptr<Room>> rooms_;
	std::unordered_map<Net::RoomId, HANDLE> roomThreads_;
	std::unordered_map<Net::RoomId, Net::RoomInfo> roomSummaries_;
	Net::RoomId nextRoomId_ = 1;
	std::queue<Net::RoomManagementEvent> managementQueue_;
	CRITICAL_SECTION managementCS_{};
	Net::PacketSender sendPacket_;
	bool stopping_ = false;
};
