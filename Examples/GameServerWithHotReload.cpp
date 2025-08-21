#include "Shared/Scripting/IScriptEngine.h"
#include "Shared/Scripting/LuaScriptEngine.h"
#include "Shared/Scripting/HotReloadManager.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Common/Logger.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

using namespace Helianthus::Scripting;
using namespace Helianthus::Network::Asio;
using namespace Helianthus::Common;

// 游戏服务器类，集成脚本热更新
class GameServer
{
public:
    GameServer() : Running(false)
    {
        // 初始化脚本引擎
        ScriptEngine = std::make_shared<LuaScriptEngine>();
        auto InitResult = ScriptEngine->Initialize();
        if (!InitResult.Success)
        {
            Logger::Error("Failed to initialize script engine: " + InitResult.ErrorMessage);
            return;
        }

        // 初始化热更新管理器
        HotReload = std::make_unique<HotReloadManager>();
        HotReload->SetEngine(ScriptEngine);
        HotReload->SetPollIntervalMs(1000); // 1秒轮询间隔
        HotReload->SetFileExtensions({".lua"});
        HotReload->SetOnFileReloaded([this](const std::string& ScriptPath, bool Success, const std::string& ErrorMessage) {
            OnScriptReloaded(ScriptPath, Success, ErrorMessage);
        });

        // 添加脚本目录监控
        HotReload->AddWatchPath("Scripts");
        HotReload->AddWatchPath("Scripts/Game");

        // 初始化网络上下文
        NetworkContext = std::make_shared<IoContext>();
    }

    ~GameServer()
    {
        Stop();
    }

    bool Start()
    {
        if (Running.exchange(true))
        {
            return true;
        }

        Logger::Info("Starting Game Server...");

        // 加载初始脚本
        LoadInitialScripts();

        // 启动热更新监控
        HotReload->Start();
        Logger::Info("Hot reload manager started");

        // 启动网络事件循环
        NetworkThread = std::thread([this]() {
            NetworkContext->Run();
        });

        // 启动游戏逻辑循环
        GameThread = std::thread([this]() {
            GameLoop();
        });

        Logger::Info("Game Server started successfully");
        return true;
    }

    void Stop()
    {
        if (!Running.exchange(false))
        {
            return;
        }

        Logger::Info("Stopping Game Server...");

        // 停止网络上下文
        if (NetworkContext)
        {
            NetworkContext->Stop();
        }

        // 停止热更新
        if (HotReload)
        {
            HotReload->Stop();
        }

        // 等待线程结束
        if (NetworkThread.joinable())
        {
            NetworkThread.join();
        }
        if (GameThread.joinable())
        {
            GameThread.join();
        }

        Logger::Info("Game Server stopped");
    }

    bool IsRunning() const
    {
        return Running.load();
    }

private:
    void LoadInitialScripts()
    {
        // 加载基础脚本
        auto Result = ScriptEngine->LoadFile("Scripts/hello.lua");
        if (Result.Success)
        {
            Logger::Info("Loaded initial script: Scripts/hello.lua");
            
            // 调用脚本中的函数
            ScriptEngine->CallFunction("Greet", {"GameServer"});
        }
        else
        {
            Logger::Warn("Failed to load initial script: " + Result.ErrorMessage);
        }

        // 加载游戏逻辑脚本
        Result = ScriptEngine->LoadFile("Scripts/Game/game_logic.lua");
        if (Result.Success)
        {
            Logger::Info("Loaded game logic script: Scripts/Game/game_logic.lua");
            
            // 初始化游戏逻辑
            ScriptEngine->CallFunction("GameLogic.Initialize", {});
            
            // 添加一些测试玩家
            ScriptEngine->CallFunction("GameLogic.AddPlayer", {"Alice"});
            ScriptEngine->CallFunction("GameLogic.AddPlayer", {"Bob"});
        }
        else
        {
            Logger::Warn("Failed to load game logic script: " + Result.ErrorMessage);
        }

        // 加载测试热更新脚本
        Result = ScriptEngine->LoadFile("Scripts/Game/test_hotreload.lua");
        if (Result.Success)
        {
            Logger::Info("Loaded test hot reload script: Scripts/Game/test_hotreload.lua");
            
            // 调用测试函数
            ScriptEngine->CallFunction("TestModule.Hello", {});
        }
        else
        {
            Logger::Warn("Failed to load test hot reload script: " + Result.ErrorMessage);
        }
    }

    void OnScriptReloaded(const std::string& ScriptPath, bool Success, const std::string& ErrorMessage)
    {
        if (Success)
        {
            Logger::Info("Script reloaded successfully: " + ScriptPath);
            
            // 根据重载的脚本类型执行不同的逻辑
            if (ScriptPath.find("hello.lua") != std::string::npos)
            {
                // 基础脚本重载
                ScriptEngine->CallFunction("Greet", {"HotReload"});
            }
            else if (ScriptPath.find("game_logic.lua") != std::string::npos)
            {
                // 游戏逻辑脚本重载
                Logger::Info("Game logic script reloaded, reinitializing...");
                ScriptEngine->CallFunction("GameLogic.Initialize", {});
                
                // 重新添加玩家（保持游戏状态）
                ScriptEngine->CallFunction("GameLogic.AddPlayer", {"Alice"});
                ScriptEngine->CallFunction("GameLogic.AddPlayer", {"Bob"});
                ScriptEngine->CallFunction("GameLogic.AddPlayer", {"Charlie"});
            }
            else if (ScriptPath.find("test_hotreload.lua") != std::string::npos)
            {
                // 测试热更新脚本重载
                Logger::Info("Test hot reload script reloaded, testing...");
                ScriptEngine->CallFunction("TestModule.Hello", {});
            }
        }
        else
        {
            Logger::Error("Script reload failed: " + ScriptPath + " - " + ErrorMessage);
        }
    }

    void GameLoop()
    {
        const auto StartTime = std::chrono::steady_clock::now();
        int TickCount = 0;

        while (Running.load())
        {
            // 游戏逻辑循环
            auto Now = std::chrono::steady_clock::now();
            auto Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Now - StartTime).count();

            // 每秒执行一次游戏逻辑
            if (Elapsed >= TickCount * 1000)
            {
                // 调用脚本中的游戏逻辑更新函数
                ScriptEngine->CallFunction("GameLogic.Update", {"1.0"});
                
                TickCount++;
                
                if (TickCount % 10 == 0) // 每10秒输出一次状态
                {
                    std::ostringstream Oss;
                    Oss << "Game Server running for " << TickCount << " seconds";
                    Logger::Info(Oss.str());
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

private:
    std::atomic<bool> Running;
    std::shared_ptr<IScriptEngine> ScriptEngine;
    std::unique_ptr<HotReloadManager> HotReload;
    std::shared_ptr<IoContext> NetworkContext;
    
    std::thread NetworkThread;
    std::thread GameThread;
};

int main()
{
    Logger::Info("Starting Helianthus Game Server with Hot Reload...");

    GameServer Server;
    
    if (!Server.Start())
    {
        Logger::Error("Failed to start game server");
        return 1;
    }

    Logger::Info("Game Server is running. Press Ctrl+C to stop.");
    Logger::Info("Modify Lua scripts in Scripts/ directory to see hot reload in action.");
    Logger::Info("Try modifying Scripts/Game/game_logic.lua to see game logic hot reload!");

    // 等待用户中断
    try
    {
        while (Server.IsRunning())
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    catch (const std::exception& E)
    {
        Logger::Error("Exception in main loop: " + std::string(E.what()));
    }

    Server.Stop();
    Logger::Info("Game Server shutdown complete");

    return 0;
}
