#include "pch.h"
#include "Framework.h"
#include "ShootingScene.h"

int main(int argc, char** argv)
{
	try
	{
		// Visual Studio와 탐색기 모두 실행 파일 폴더를 기준으로 리소스를 읽습니다.
		std::array<wchar_t, 32768> executable{};
		const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));

		if (length == 0 || length >= executable.size())
		{
			throw std::runtime_error("Cannot locate executable");
		}

		std::filesystem::current_path(std::filesystem::path(executable.data()).parent_path());

		Framework framework;

		return framework.Run(argc, argv, std::make_unique<ShootingScene>());
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';

		return 1;
	}
}
