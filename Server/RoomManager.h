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
#include <unordered_map>

class Room;

class RoomManager
{
public:
	explicit RoomManager(Net::PacketSender sender = {});
	~RoomManager();
	RoomManager(const RoomManager&) = delete;
	RoomManager& operator=(const RoomManager&) = delete;

	// 운영 1인 방의 입장 정보는 서버 메인이 확인해서 전달한다.
	Net::RoomId CreateGameRoom(const Net::SessionToken& creator,
		Net::MembershipId membershipId, bool UDPRegistered, std::uint32_t requestId = 0);
	bool AddCommand(const Net::RoomCommand& command);
	bool HasRoom(Net::RoomId roomId) const;
	// 요청 전달 중지와 방 스레드 종료를 확인한 뒤 방 객체를 삭제한다.
	void DeleteRoom(Net::RoomId roomId);
	void StopRooms();
	void UpdateRooms();

private:
	std::unordered_map<Net::RoomId, std::unique_ptr<Room>> rooms_;
	std::unordered_map<Net::RoomId, Net::RoomInfo> roomSummaries_;
	Net::RoomId nextRoomId_ = 1;
	Net::PacketSender sendPacket_;
	bool stopping_ = false;
};
