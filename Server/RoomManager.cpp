#include "pch.h"
#include "RoomManager.h"
#include "Room.h"
#include <utility>

RoomManager::RoomManager(Net::PacketSender sender) : sendPacket_(std::move(sender))
{
}

RoomManager::~RoomManager()
{
	StopRooms();
}

Net::RoomId RoomManager::CreateGameRoom(const Net::SessionToken& creator,
	Net::MembershipId membershipId, bool UDPRegistered, std::uint32_t requestId)
{
	if (stopping_ || !sendPacket_ || !Net::IsValidSessionToken(creator) || membershipId == 0
		|| !UDPRegistered || rooms_.size() >= Net::MaxRooms || nextRoomId_ == 0)
		return 0;

	Net::RoomId roomId = nextRoomId_++;
	try
	{
		std::unique_ptr<Room> room = std::make_unique<Room>(roomId, sendPacket_);
		Net::RoomCommand command;
		command.sessionToken = creator;
		command.requestId = requestId;
		command.context.roomId = roomId;
		command.membershipId = membershipId;
		command.receivedAtMs = GetTickCount64();
		command.body = Net::RoomEnterCommand{UDPRegistered};
		if (!room->AddCommand(command))
			return 0;
		auto roomEntry = rooms_.emplace(roomId, std::move(room)).first;
		Net::RoomInfo summary;
		summary.roomId = roomId;
		summary.capacity = 1;
		roomSummaries_.emplace(roomId, summary);
		if (roomEntry->second->StartRoomThread())
			return roomId;
	}
	catch (...)
	{
		fputs("1인 방을 생성하지 못했습니다.\n", stderr);
	}
	DeleteRoom(roomId);
	return 0;
}

bool RoomManager::AddCommand(const Net::RoomCommand& command)
{
	if (stopping_)
		return false;
	auto room = rooms_.find(command.context.roomId);
	return room != rooms_.end() && room->second->AddCommand(command);
}

bool RoomManager::HasRoom(Net::RoomId roomId) const
{
	return rooms_.find(roomId) != rooms_.end();
}

void RoomManager::DeleteRoom(Net::RoomId roomId)
{
	auto room = rooms_.find(roomId);
	if (room == rooms_.end())
		return;
	room->second->StopRoom();
	// 방의 임계 영역과 상태는 스레드가 끝난 뒤 해제한다.
	rooms_.erase(room);
	roomSummaries_.erase(roomId);
}

void RoomManager::StopRooms()
{
	stopping_ = true;
	for (const auto& entry : rooms_)
		entry.second->RequestStop();
	while (!rooms_.empty())
		DeleteRoom(rooms_.begin()->first);
}

void RoomManager::UpdateRooms()
{
	for (auto room = rooms_.begin(); room != rooms_.end();)
	{
		std::queue<Net::RoomManagementEvent> events = room->second->TakeStateEvents();
		++room;
		while (!events.empty())
		{
			Net::RoomManagementEvent& event = events.front();
			auto summary = roomSummaries_.find(event.summary.roomId);
			if (summary != roomSummaries_.end())
			{
				if (event.type == Net::RoomManagementEventType::RoomEmpty)
					DeleteRoom(event.summary.roomId);
				else
					summary->second = event.summary;
			}
			events.pop();
		}
	}
	// 예외 등으로 먼저 끝난 방도 핸들을 합류시킨 뒤 정리한다.
	for (auto room = rooms_.begin(); room != rooms_.end();)
	{
		Net::RoomId roomId = room->first;
		bool ended = room->second->IsThreadFinished();
		++room;
		if (ended)
			DeleteRoom(roomId);
	}
}
