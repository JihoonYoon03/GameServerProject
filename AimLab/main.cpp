#include "pch.h"
#include "Framework.h"
#include "ShootingScene.h"
#include "NetworkClient.h"

namespace
{
	bool ReadPort(const std::string& text, std::uint16_t& port)
	{
		unsigned int value = 0;
		if (text.empty())
			return false;
		for (char digit : text)
		{
			if (digit < '0' || digit > '9')
				return false;
			value = value * 10 + digit - '0';
			if (value > 65535)
				return false;
		}
		if (value == 0)
			return false;
		port = static_cast<std::uint16_t>(value);
		return true;
	}
}

int main(int argc, char** argv)
{
	try
	{
		// Visual Studio와 탐색기 모두 실행 파일 폴더를 기준으로 리소스를 읽습니다.
		std::array<wchar_t, 32768> executable{};
		DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));

		if (length == 0 || length >= executable.size())
		{
			throw std::runtime_error("Cannot locate executable");
		}

		std::filesystem::current_path(std::filesystem::path(executable.data()).parent_path());

		std::string address;
		std::string portText;
		if (argc == 3)
		{
			address = argv[1];
			portText = argv[2];
		}
		else if (argc == 1)
		{
			std::cout << "서버 IPv4 주소와 TCP 포트를 입력하세요: ";
			if (!(std::cin >> address >> portText))
				throw std::runtime_error("IP 주소와 포트 입력을 읽지 못했습니다.");
		}
		else
			throw std::runtime_error("Usage: AimLab.exe SERVER_IP TCP_PORT");

		std::uint16_t tcpPort = 0;
		if (!ReadPort(portText, tcpPort))
			throw std::runtime_error("TCP 포트는 1~65535 사이여야 합니다.");
		std::unique_ptr<NetworkClient> network = std::make_unique<NetworkClient>();
		if (!network->ConnectServer(address, tcpPort) || !network->StartNetwork())
			throw std::runtime_error("서버 연결을 시작하지 못했습니다.");
		std::cout << "서버 접속 및 최초 UDP 등록을 기다립니다.\n";
		Framework framework(std::move(network));
		// 접속 인자는 GLUT가 해석하지 않도록 실행 파일 이름만 전달한다.
		int glutArgc = 1;
		char* glutArgv[] = {argv[0], nullptr};
		return framework.Run(glutArgc, glutArgv, std::make_unique<ShootingScene>());
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';

		return 1;
	}
}
