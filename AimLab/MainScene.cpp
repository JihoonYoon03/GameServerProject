#include "pch.h"
#include "MainScene.h"
#include "NetworkClient.h"
#include "../Shared/PacketCodec.h"

namespace
{
    constexpr std::size_t RoomsPerPage = 6;
    constexpr HudRect JoinButton{-0.82f, -0.78f, -0.30f, -0.59f};
    constexpr HudRect CreateButton{-0.26f, -0.78f, 0.26f, -0.59f};
    constexpr HudRect RefreshButton{0.30f, -0.78f, 0.82f, -0.59f};
    constexpr HudRect PrevButton{0.16f, 0.64f, 0.45f, 0.81f};
    constexpr HudRect NextButton{0.49f, 0.64f, 0.82f, 0.81f};

    HudRect RoomRow(std::size_t index)
    {
        float top = 0.54f - 0.158f * static_cast<float>(index % RoomsPerPage);
        return {-0.82f, top - 0.14f, 0.82f, top};
    }
}

MainScene::MainScene(NetworkClient& network) : network_(&network)
{
}

void MainScene::Initialize()
{
    SearchRoomList();
}

void MainScene::SearchRoomList()
{
    if (pendingListRequestId_ != 0)
        return;

    Net::PacketData packet;
    std::uint32_t requestId = network_->NextRequestId();
    if (!Net::PacketCodec::EncodeTcp(Net::RoomListRequest{}, requestId, packet) || !network_->SendPacket(packet))
        throw std::runtime_error("방 목록을 요청하지 못했습니다.");
    pendingListRequestId_ = requestId;
    notice_.clear();
}

void MainScene::UpdateRoomList(std::uint32_t requestId, const Net::RoomListResult& result)
{
    if (pendingListRequestId_ == 0 || requestId != pendingListRequestId_)
        return;

    // 새로고침으로 행 순서가 바뀌어도 선택했던 방 번호를 유지한다.
    Net::RoomId previousRoom = roomList_.empty() ? 0 : roomList_[selected_].roomId;
    pendingListRequestId_ = 0;
    roomList_ = result.rooms;
    std::sort(roomList_.begin(), roomList_.end(), [](const Net::RoomInfo& a, const Net::RoomInfo& b)
    {
        return a.roomId < b.roomId;
    });
    selected_ = 0;
    for (std::size_t i = 0; i < roomList_.size(); ++i)
    {
        if (roomList_[i].roomId == previousRoom)
        {
            selected_ = i;
            break;
        }
    }
    ShowRoomList();
    if (firstList_ && roomList_.empty())
        RequestEntry(0);
    firstList_ = false;
}

void MainScene::RequestEntry(Net::RoomId roomId)
{
    if (!network_->RequestGameRoom(roomId))
        throw std::runtime_error("방 입장 요청을 보내지 못했습니다.");
    entering_ = true;
}

bool MainScene::JoinRoom(const Net::RoomEnterResult& result)
{
    if (!entering_)
        return false;
    entering_ = false;
    if (result.result == Net::ErrorCode::Ok)
    {
        roomId_ = result.room.version.context.roomId;
        leaveFailed_ = false;
        notice_.clear();
        return true;
    }

    // 갱신 중 방이 사라져도 어느 방에서 입장이 거절됐는지 알 수 있게 한다.
    std::string roomName = roomList_.empty() ? "Room" : "Room " + std::to_string(roomList_[selected_].roomId);
    SearchRoomList();
    switch (result.result)
    {
    case Net::ErrorCode::RoomNotFound: notice_ = roomName + " no longer exists"; break;
    case Net::ErrorCode::RoomFull:     notice_ = roomName + " is full"; break;
    case Net::ErrorCode::RoomPlaying:  notice_ = roomName + " already started"; break;
    default: notice_ = "Entry refused (" + std::to_string(static_cast<unsigned>(result.result)) + ")"; break;
    }
    return false;
}

void MainScene::ExitRoom()
{
    if (roomId_ == 0 || IsLeaving())
        return;

    pendingLeaveRequestId_ = network_->RequestLeaveRoom(roomId_);
    leaveFailed_ = !IsLeaving();
}

bool MainScene::OnRoomExited(std::uint32_t requestId, const Net::LeaveRoomResult& result)
{
    if (!IsLeaving() || requestId != pendingLeaveRequestId_ || result.roomId != roomId_)
        return false;

    if (result.result != Net::ErrorCode::Ok)
    {
        OnLeaveRejected(requestId);
        return false;
    }

    pendingLeaveRequestId_ = 0;
    roomId_ = 0;
    SearchRoomList();
    return true;
}

bool MainScene::OnLeaveRejected(std::uint32_t requestId)
{
    if (!IsLeaving() || requestId != pendingLeaveRequestId_)
        return false;

    pendingLeaveRequestId_ = 0;
    leaveFailed_ = true;
    return true;
}

bool MainScene::IsLeaving() const
{
    return pendingLeaveRequestId_ != 0;
}

const char* MainScene::LeaveButtonText() const
{
    if (IsLeaving())
        return "Leaving...";
    if (leaveFailed_)
        return "Retry leave [L]";
    return "Leave room [L]";
}

void MainScene::SelectRoom(std::size_t index)
{
    selected_ = index;
    notice_.clear();
}

bool MainScene::CanJoinSelectedRoom() const
{
    if (roomList_.empty())
        return false;
    const Net::RoomInfo& room = roomList_[selected_];
    return room.state == Net::GameState::Preparing && room.playerCount < room.capacity;
}

void MainScene::Update(float, const Input& input)
{
    if (entering_ || pendingListRequestId_ != 0)
        return;
    if (input.WasPressed('n') || input.WasPressed('N'))
        RequestEntry(0);
    else if (input.WasPressed('r') || input.WasPressed('R'))
        SearchRoomList();
    else if (!roomList_.empty())
    {
        if (input.WasPressed('w') || input.WasPressed('W'))
            SelectRoom((selected_ + roomList_.size() - 1) % roomList_.size());
        else if (input.WasPressed('s') || input.WasPressed('S'))
            SelectRoom((selected_ + 1) % roomList_.size());
        else if (input.WasPressed('\r') && CanJoinSelectedRoom())
            RequestEntry(roomList_[selected_].roomId);
    }
}

void MainScene::PickButton(float x, float y)
{
    if (entering_ || pendingListRequestId_ != 0)
        return;

    const std::size_t first = selected_ / RoomsPerPage * RoomsPerPage;
    if (CreateButton.Contains(x, y))
        RequestEntry(0);
    else if (RefreshButton.Contains(x, y))
        SearchRoomList();
    else if (JoinButton.Contains(x, y) && CanJoinSelectedRoom())
        RequestEntry(roomList_[selected_].roomId);
    else if (PrevButton.Contains(x, y) && first >= RoomsPerPage)
        SelectRoom(first - RoomsPerPage);
    else if (NextButton.Contains(x, y) && first + RoomsPerPage < roomList_.size())
        SelectRoom(first + RoomsPerPage);
    else
    {
        for (std::size_t i = first; i < std::min(first + RoomsPerPage, roomList_.size()); ++i)
        {
            if (RoomRow(i).Contains(x, y))
            {
                SelectRoom(i);
                break;
            }
        }
    }
}

Hud MainScene::GetHud() const
{
    Hud hud;
    hud.crosshair = false;
    hud.message = notice_;
    const bool available = !entering_ && pendingListRequestId_ == 0;
    if (entering_)
        hud.message = "Entering room...";
    else if (pendingListRequestId_ != 0 && notice_.empty())
        hud.message = "Loading room list...";
    else if (roomList_.empty() && notice_.empty())
        hud.message = "No rooms available";

    const std::size_t first = selected_ / RoomsPerPage * RoomsPerPage;
    std::string title = "Rooms (" + std::to_string(roomList_.size()) + ")";
    if (roomList_.size() > RoomsPerPage)
    {
        title += "    " + std::to_string(first / RoomsPerPage + 1) + "/"
            + std::to_string((roomList_.size() + RoomsPerPage - 1) / RoomsPerPage);
        hud.lobbyButtons.push_back({PrevButton, "Prev", available && first > 0});
        hud.lobbyButtons.push_back({NextButton, "Next", available && first + RoomsPerPage < roomList_.size()});
    }
    hud.lines.push_back(title);

    for (std::size_t i = first; i < std::min(first + RoomsPerPage, roomList_.size()); ++i)
    {
        const Net::RoomInfo& room = roomList_[i];
        std::string state = "Waiting";
        if (room.state == Net::GameState::Playing) state = "Playing";
        else if (room.state == Net::GameState::Results) state = "Results";
        std::string label = "Room " + std::to_string(room.roomId) + "      "
            + std::to_string(room.playerCount) + "/" + std::to_string(room.capacity) + "      " + state;
        hud.lobbyButtons.push_back({RoomRow(i), label, available, i == selected_});
    }
    hud.lobbyButtons.push_back({JoinButton, "Join [Enter]", available && CanJoinSelectedRoom()});
    hud.lobbyButtons.push_back({CreateButton, "New room [N]", available});
    hud.lobbyButtons.push_back({RefreshButton, "Refresh [R]", available});
    return hud;
}

void MainScene::ShowRoomList() const
{
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    bool isConsole = GetConsoleMode(console, &mode) != 0;
    const wchar_t* newline = isConsole ? L"\r\n" : L"\n";
    std::wostringstream list;
    list << L"방 목록 (" << roomList_.size() << L"개)" << newline;
    for (const Net::RoomInfo& room : roomList_)
    {
        const wchar_t* state = L"대기";
        if (room.state == Net::GameState::Playing)
            state = L"경기 중";
        else if (room.state == Net::GameState::Results)
            state = L"결과";
        list << L"  방 " << room.roomId << L" | " << static_cast<int>(room.playerCount)
            << L'/' << static_cast<int>(room.capacity) << L"명 | " << state << newline;
    }
    if (!firstList_ || !roomList_.empty())
        list << L"게임 창에서 방과 버튼을 클릭하세요. W/S로 방 선택, Enter로 입장, N으로 새 방 생성, R로 목록 갱신" << newline;

    std::wstring text = list.str();
    if (isConsole)
    {
        // 콘솔에는 코드 페이지와 관계없이 한글을 직접 쓴다.
        std::cout.flush();
        DWORD written = 0;
        if (WriteConsoleW(console, text.data(), static_cast<DWORD>(text.size()), &written, nullptr))
            return;
    }

    // 파이프에서는 파워셸의 출력 인코딩으로 바꿔 게임 실행 중에도 목록을 전달한다.
    UINT codePage = GetConsoleOutputCP();
    if (codePage == 0)
        codePage = GetACP();
    int length = static_cast<int>(text.size());
    int bytes = WideCharToMultiByte(codePage, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
    std::string output(bytes, '\0');
    if (bytes > 0 && WideCharToMultiByte(codePage, 0, text.data(), length, output.data(), bytes, nullptr, nullptr))
        std::cout << output << std::flush;
}
