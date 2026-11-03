#pragma once
#include "../Shared/NetworkTypes.h"

class ClientPlayer
{
public:
	Net::PlayerData state;
	Net::Sequence lastProcessedInput = 0;
	Net::ShotId lastProcessedShot = 0;
};

