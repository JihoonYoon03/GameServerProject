#include "pch.h"
#include "Framework.h"
#include "Client.h"
#include "ClientPlayer.h"
#include "Framework.h"
#include "NetworkServer.h"
#include "Room.h"
#include "RoomManager.h"
#include "StageController.h"
#include "Target.h"

// 클라이언트·서버 선언을 함께 컴파일해 이름·자료형 충돌을 확인한다. 통신 함수는 실행하지 않는다.
static_assert(Net::MaxPlayers == 4, "Room capacity differs from the plan");
static_assert(Net::TcpHeaderBytes == 10 && Net::UdpHeaderBytes == 40, "Header sizes differ from the plan");
