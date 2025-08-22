#include "Shared/Scripting/IScriptEngine.h"
#include "Shared/Scripting/LuaScriptEngine.h"
#include "Shared/Scripting/HotReloadManager.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using namespace Helianthus::Scripting;

int main()
{
	auto Engine = std::make_shared<LuaScriptEngine>();
	auto Init = Engine->Initialize();
	if (!Init.Success)
	{
		std::cerr << "Initialize failed: " << Init.ErrorMessage << std::endl;
		return 1;
	}

	// 预加载一个脚本文件（可修改成你的路径）
	Engine->LoadFile("Scripts/hello.lua");

	HotReloadManager Manager;
	Manager.SetEngine(Engine);
	Manager.SetPollIntervalMs(500);
	Manager.SetFileExtensions({".lua"});
	Manager.SetOnFileReloaded([](const std::string& ScriptPath, bool Success, const std::string& ErrorMessage) {
		std::cout << "Reloaded: " << ScriptPath << ", success=" << (Success ? "true" : "false");
		if (!Success)
		{
			std::cout << ", error=" << ErrorMessage;
		}
		std::cout << std::endl;
	});
	Manager.AddWatchPath("Scripts");
	Manager.Start();

	std::cout << "Lua hot-reload running. Modify files in Scripts/ to see reloads. Press Ctrl+C to exit.\n";
	for (;;)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}




