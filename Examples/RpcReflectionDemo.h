#pragma once

#include "Shared/RPC/IRpcServer.h"
#include "Shared/RPC/RpcReflection.h"
#include "Shared/Reflection/ReflectionMacros.h"
#include "Shared/Common/LogCategories.h"
#include <string>

// 示例：用户管理服务
HCLASS(RpcService)
class UserService : public Helianthus::RPC::RpcServiceBase
{
    GENERATED_BODY()
public:
    UserService() : RpcServiceBase("UserService") {}

    // 公共RPC方法 - 会被自动挂载
    HMETHOD(Rpc) std::string GetUser(const std::string& UserId);
    HMETHOD(Rpc) void UpdateUser(const Helianthus::RPC::RpcContext& Ctx, const std::string& UserData, Helianthus::RPC::RpcCallback Callback);
    
    // 管理员方法 - 需要Admin标签
    HMETHOD(Admin) void DeleteUser(const std::string& UserId);
    
    // 游戏相关方法 - 需要Game标签
    HMETHOD(Game) std::string JoinGame(const std::string& PlayerId);
    HMETHOD(Game) void LeaveGame(const std::string& PlayerId);
    
    // 管理员游戏方法
    HMETHOD(Admin) void KickPlayer(const std::string& PlayerId);
    
    // 统计方法 - 需要Stats标签
    HMETHOD(Stats) std::string GetPlayerStats(const std::string& PlayerId);
    HMETHOD(Stats) std::string GetServerStats(const std::string& ServerId);

    HRPC_FACTORY()

private:
    std::string UserId;
    std::string UserData;
};

// 示例：游戏服务
HCLASS(RpcService)
class GameService : public Helianthus::RPC::RpcServiceBase
{
    GENERATED_BODY()
public:
    GameService() : RpcServiceBase("GameService") {}

    // 游戏相关RPC方法
    HMETHOD(Game) std::string JoinGame(const std::string& PlayerId);
    HMETHOD(Game) void LeaveGame(const std::string& PlayerId);
    HMETHOD(Game) std::string GetGameState(const std::string& GameId);
    
    // 管理员方法
    HMETHOD(Admin) void CreateGame(const std::string& GameConfig);
    HMETHOD(Admin) void DeleteGame(const std::string& GameId);
    
    // 统计方法
    HMETHOD(Stats) std::string GetGameStats(const std::string& GameId);

    HRPC_FACTORY()

private:
    std::string PlayerId;
};

// 示例：统计服务
HCLASS(RpcService)
class StatsService : public Helianthus::RPC::RpcServiceBase
{
    GENERATED_BODY()
public:
    StatsService() : RpcServiceBase("StatsService") {}

    // 统计相关方法
    HMETHOD(Stats) std::string GetPlayerStats(const std::string& PlayerId);
    HMETHOD(Stats) std::string GetServerStats(const std::string& ServerId);
    HMETHOD(Stats) std::string GetGameStats(const std::string& GameId);
    
    // 管理员统计方法
    HMETHOD(Admin) void ResetStats(const std::string& TargetId);
    HMETHOD(Admin) void ExportStats(const std::string& Format);

    HRPC_FACTORY()

private:
    std::string PlayerId;
};
