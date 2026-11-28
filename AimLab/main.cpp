#include "pch.h"
#include "Framework.h"
#include "TitleScene.h"

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

		std::string address = "127.0.0.1";
		std::string portText = "5000";
		if (argc == 3)
		{
			address = argv[1];
			portText = argv[2];
		}
		else if (argc != 1)
			throw std::runtime_error("Usage: AimLab.exe SERVER_IP TCP_PORT");

		std::uint16_t tcpPort = 0;
		if (!ReadPort(portText, tcpPort))
			throw std::runtime_error("TCP port must be between 1 and 65535.");
		Framework framework(address, tcpPort);
		auto scene = std::make_unique<TitleScene>(address);
		int glutArgc = 1;
		char* glutArgv[] = {argv[0], nullptr};
		return framework.Run(glutArgc, glutArgv, std::move(scene));
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';

		return 1;
	}
}
