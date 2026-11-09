#include "../Server/Room.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    Net::RoomCommand Command(std::uint32_t requestId)
    {
        Net::RoomCommand command;
        command.requestId = requestId;
        command.sessionToken[0] = 7;
        command.context = {3, 4};
        command.membershipId = 5;
        command.receivedAtMs = 600;
        command.body = Net::ReloadRequest{command.context, command.membershipId};
        return command;
    }

    void TestCommandOrder()
    {
        std::vector<std::uint32_t> calls;
        bool receivedOriginalFields = false;
        Room room;
        room.SetHandlers([&](const Net::RoomCommand& command)
        {
            calls.push_back(command.requestId);
            if (command.requestId == 17)
            {
                receivedOriginalFields = command.sessionToken[0] == 7 && command.context.roomId == 3 &&
                    command.context.matchId == 4 && command.membershipId == 5 && command.receivedAtMs == 600 &&
                    std::holds_alternative<Net::ReloadRequest>(command.body);
                room.AddCommand(Command(19));
            }
        }, [&]() { calls.push_back(0); });

        room.AddCommand(Command(17));
        room.AddCommand(Command(18));
        room.Tick();
        Check(calls == std::vector<std::uint32_t>{17, 18, 0}, "commands run in order before the tick callback");
        Check(receivedOriginalFields, "command fields reach the callback unchanged");

        // 명령 콜백이 넣은 요청은 다음 갱신에서 처리한다.
        room.Tick();
        Check(calls == std::vector<std::uint32_t>{17, 18, 0, 19, 0}, "commands added during a tick wait for the next tick");

        room.AddCommand(Command(20));
        room.StopRoom();
        room.Tick();
        Check(calls.size() == 5, "stop cancels pending commands and ticks");
    }

    void TestRoomThread()
    {
        std::atomic<int> processed{0};
        std::atomic<int> ticks{0};
        std::atomic<std::uint32_t> lastRequest{0};
        std::atomic<DWORD> workerThread{0};
        std::atomic<bool> multipleThreads{false};
        std::atomic<ULONGLONG> firstTick{0};
        std::atomic<ULONGLONG> seventhTick{0};

        Target targets;
        Room room({3, 4}, targets, {}, {});
        room.SetHandlers([&](const Net::RoomCommand& command)
        {
            lastRequest = command.requestId;
            ++processed;
        }, [&]()
        {
            DWORD currentThread = GetCurrentThreadId();
            DWORD previousThread = workerThread.exchange(currentThread);
            if (previousThread != 0 && previousThread != currentThread)
                multipleThreads = true;

            int count = ++ticks;
            if (count == 1)
                firstTick = GetTickCount64();
            else if (count == 7)
                seventhTick = GetTickCount64();
        });

        room.AddCommand(Command(17));
        room.StartRoomThread();
        room.StartRoomThread();
        ULONGLONG deadline = GetTickCount64() + 2000;
        while (seventhTick == 0 && GetTickCount64() < deadline)
            Sleep(5);
        room.StopRoom();

        Check(seventhTick != 0 && firstTick != 0, "room worker reaches seven ticks");
        Check(workerThread != GetCurrentThreadId() && !multipleThreads, "only one room worker runs");
        Check(processed == 1 && lastRequest == 17, "queued command reaches the room worker");
        // 처음과 일곱 번째 갱신 사이의 기준값은 200밀리초다. 스케줄러 지연을 허용한다.
        ULONGLONG elapsed = seventhTick.load() - firstTick.load();
        Check(elapsed >= 130 && elapsed <= 450, "room worker runs near 30 ticks per second");

        int stoppedTicks = ticks;
        room.StopRoom();
        Sleep(70);
        Check(ticks == stoppedTicks, "stopped room no longer ticks");

        room.AddCommand(Command(18));
        room.StartRoomThread();
        room.AddCommand(Command(19));
        deadline = GetTickCount64() + 2000;
        while (processed < 2 && GetTickCount64() < deadline)
            Sleep(5);
        room.StopRoom();
        Check(processed == 2 && lastRequest == 19, "restart only receives commands from the new run");
    }

    void TestDestructorWaits()
    {
        std::atomic<bool> entered{false};
        std::atomic<bool> finished{false};
        {
            Room room;
            room.SetHandlers({}, [&]()
            {
                entered = true;
                Sleep(40);
                finished = true;
            });
            room.StartRoomThread();
            ULONGLONG deadline = GetTickCount64() + 2000;
            while (!entered && GetTickCount64() < deadline)
                Sleep(5);
            Check(entered, "room worker enters its callback");
        }
        Check(finished, "room destruction waits for the running callback");
    }

    void TestCallbackExceptions()
    {
        std::atomic<int> commands{0};
        std::atomic<int> ticks{0};
        Room room;
        room.SetHandlers([&](const Net::RoomCommand&)
        {
            ++commands;
            throw std::runtime_error("command failure");
        }, [&]() { ++ticks; });
        room.AddCommand(Command(17));
        room.AddCommand(Command(18));
        room.StartRoomThread();

        ULONGLONG deadline = GetTickCount64() + 2000;
        while (commands == 0 && GetTickCount64() < deadline)
            Sleep(5);
        Sleep(70);
        room.StopRoom();
        Check(commands == 1 && ticks == 0, "command failure stops the worker and its pending work");

        room.SetHandlers({}, [&]()
        {
            ++ticks;
            throw 7;
        });
        room.StartRoomThread();
        deadline = GetTickCount64() + 2000;
        while (ticks == 0 && GetTickCount64() < deadline)
            Sleep(5);
        Sleep(70);
        room.StopRoom();
        Check(ticks == 1, "restarted worker stops on a tick failure");
    }

    void TestCallbackRequestsStop()
    {
        std::atomic<int> processed{0};
        std::atomic<int> ticks{0};
        std::atomic<bool> callbackReturned{false};
        std::atomic<std::uint32_t> lastRequest{0};
        Room room;
        room.SetHandlers([&](const Net::RoomCommand&)
        {
            ++processed;
            room.AddCommand(Command(20));
            room.StopRoom();
            callbackReturned = true;
        }, [&]() { ++ticks; });
        room.AddCommand(Command(17));
        room.AddCommand(Command(18));
        room.StartRoomThread();

        ULONGLONG deadline = GetTickCount64() + 2000;
        while (!callbackReturned && GetTickCount64() < deadline)
            Sleep(5);
        if (!callbackReturned)
        {
            // 방 스레드가 자신을 기다리면 소멸자도 대기하므로 바로 테스트를 종료한다.
            std::fputs("FAIL: room callback did not return from StopRoom\n", stderr);
            std::exit(EXIT_FAILURE);
        }
        room.StopRoom();
        Check(processed == 1 && ticks == 0, "callback stop skips subsequent commands and ticks");

        room.SetHandlers([&](const Net::RoomCommand& command)
        {
            lastRequest = command.requestId;
            ++processed;
        }, {});
        room.StartRoomThread();
        room.AddCommand(Command(21));
        deadline = GetTickCount64() + 2000;
        while (processed < 2 && GetTickCount64() < deadline)
            Sleep(5);
        room.StopRoom();
        Check(processed == 2 && lastRequest == 21, "manager stop clears queued work before restart");
    }
}

int main()
{
    try
    {
        TestCommandOrder();
        TestRoomThread();
        TestDestructorWaits();
        TestCallbackExceptions();
        TestCallbackRequestsStop();
        std::puts("PASS: room queue, timing, callback failure, shutdown and restart");
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::fprintf(stderr, "FAIL: %s\n", exception.what());
        return 1;
    }
}
