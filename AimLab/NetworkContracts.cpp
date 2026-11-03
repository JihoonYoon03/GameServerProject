#include "pch.h"
#include "Framework.h"
#include "MainScene.h"
#include "NetworkClient.h"
#include "ShootingScene.h"
#include "Shared/NetworkConversions.h"

// 클라이언트·서버 선언을 함께 컴파일해 이름·자료형 충돌을 확인한다. 통신 함수는 실행하지 않는다.
static_assert(Net::MaxPlayers == 4, "Room capacity differs from the plan");
static_assert(Net::ProtocolVersion == 5, "Protocol version differs from the plan");
