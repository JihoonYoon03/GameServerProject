#include "pch.h"
#include "MainScene.h"
#include "NetworkClient.h"
#include "../Shared/PacketCodec.h"

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
}

void MainScene::UpdateRoomList(std::uint32_t requestId, const Net::RoomListResult& result)
{
    if (pendingListRequestId_ == 0 || requestId != pendingListRequestId_)
        return;

    pendingListRequestId_ = 0;
    roomList_ = result.rooms;
    std::sort(roomList_.begin(), roomList_.end(), [](const Net::RoomInfo& a, const Net::RoomInfo& b)
    {
        return a.roomId < b.roomId;
    });
    selected_ = 0;
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
    switch (result.result)
    {
    case Net::ErrorCode::Ok:           return true;
    case Net::ErrorCode::RoomNotFound: notice_ = "Room no longer exists"; break;
    case Net::ErrorCode::RoomFull:     notice_ = "Room is full"; break;
    case Net::ErrorCode::RoomPlaying:  notice_ = "Game already started"; break;
    default: notice_ = "Entry refused (" + std::to_string(static_cast<unsigned>(result.result)) + ")"; break;
    }
    SearchRoomList();
    return false;
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
            selected_ = (selected_ + roomList_.size() - 1) % roomList_.size();
        else if (input.WasPressed('s') || input.WasPressed('S'))
            selected_ = (selected_ + 1) % roomList_.size();
        else if (input.WasPressed('\r'))
            RequestEntry(roomList_[selected_].roomId);
    }
}

Hud MainScene::GetHud() const
{
    Hud hud;
    hud.crosshair = false;
    hud.messageScale = 0.00075f;
    hud.message = notice_;
    if (entering_)
        hud.lines.push_back("Entering room...");
    else if (pendingListRequestId_ != 0)
        hud.lines.push_back("Loading room list...");
    else
    {
        if (roomList_.empty())
            hud.lines.push_back("No rooms available");
        else
        {
            const Net::RoomInfo& room = roomList_[selected_];
            std::string state = "Waiting";
            if (room.state == Net::GameState::Playing) state = "Playing";
            else if (room.state == Net::GameState::Results) state = "Results";
            hud.lines.push_back("Room " + std::to_string(room.roomId) + "    "
                + std::to_string(room.playerCount) + "/" + std::to_string(room.capacity) + "    " + state);
        }
        hud.lines.push_back("W/S: select   Enter: join   N: new   R: refresh");
    }
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
        list << L"게임 창에서 W/S로 방 선택, Enter로 입장, N으로 새 방 생성, R로 목록 갱신" << newline;

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
