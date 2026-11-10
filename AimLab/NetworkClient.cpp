#include "pch.h"
#include "NetworkClient.h"

void NetworkClient::RegisterUDP()
{
}

void NetworkClient::CompleteConnection(const Net::UdpRegisterResult&)
{
}

bool NetworkClient::StartNetwork()
{
	return false;
}

void NetworkClient::NetworkLoop()
{
}

DWORD WINAPI NetworkClient::NetworkProc(LPVOID)
{
	return 0;
}
