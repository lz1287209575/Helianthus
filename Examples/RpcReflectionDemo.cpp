#include "RpcReflectionDemo.h"
#include "generated/reflection_gen.h"
#include <iostream>
#include <memory>
#include <string>

// 实现UserService的方法
std::string UserService::GetUser(const std::string& UserId)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "GetUser called with UserId: {}", UserId);
    return "User data for: " + UserId;
}

void UserService::UpdateUser(const Helianthus::RPC::RpcContext& Ctx, const std::string& UserData, Helianthus::RPC::RpcCallback Callback)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "UpdateUser called with UserData: {}", UserData);
    Callback(Helianthus::RPC::RpcResult::SUCCESS, "User updated successfully");
}

void UserService::DeleteUser(const std::string& UserId)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Warning, "DeleteUser called for UserId: {}", UserId);
}

std::string UserService::JoinGame(const std::string& PlayerId)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "JoinGame called for PlayerId: {}", PlayerId);
    return "Player " + PlayerId + " joined game";
}

void UserService::LeaveGame(const std::string& PlayerId)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "LeaveGame called for PlayerId: {}", PlayerId);
}

void UserService::KickPlayer(const std::string& PlayerId)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Warning, "KickPlayer called for PlayerId: {}", PlayerId);
}

std::string UserService::GetPlayerStats(const std::string& PlayerId)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "GetPlayerStats called for PlayerId: {}", PlayerId);
    return "Stats for player: " + PlayerId;
}

std::string UserService::GetServerStats(const std::string& ServerId)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "GetServerStats called for ServerId: {}", ServerId);
    return "Server stats for: " + ServerId;
}

// 实现GameService的方法
std::string GameService::JoinGame(const std::string& PlayerId)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "GameService::JoinGame called for PlayerId: {}", PlayerId);
    return "Player " + PlayerId + " joined game via GameService";
}

void GameService::LeaveGame(const std::string& PlayerId)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "GameService::LeaveGame called for PlayerId: {}", PlayerId);
}

std::string GameService::GetGameState(const std::string& GameId)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "GetGameState called for GameId: {}", GameId);
    return "Game state for: " + GameId;
}

void GameService::CreateGame(const std::string& GameConfig)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "CreateGame called with config: {}", GameConfig);
}

void GameService::DeleteGame(const std::string& GameId)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Warning, "DeleteGame called for GameId: {}", GameId);
}

std::string GameService::GetGameStats(const std::string& GameId)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "GetGameStats called for GameId: {}", GameId);
    return "Game stats for: " + GameId;
}

// 实现StatsService的方法
std::string StatsService::GetPlayerStats(const std::string& PlayerId)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "StatsService::GetPlayerStats called for PlayerId: {}", PlayerId);
    return "Detailed stats for player: " + PlayerId;
}

std::string StatsService::GetServerStats(const std::string& ServerId)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "StatsService::GetServerStats called for ServerId: {}", ServerId);
    return "Detailed server stats for: " + ServerId;
}

std::string StatsService::GetGameStats(const std::string& GameId)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "StatsService::GetGameStats called for GameId: {}", GameId);
    return "Detailed game stats for: " + GameId;
}

void StatsService::ResetStats(const std::string& TargetId)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Warning, "ResetStats called for TargetId: {}", TargetId);
}

void StatsService::ExportStats(const std::string& Format)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "ExportStats called with format: {}", Format);
}

int main()
{
    std::cout << "=== RPC反射系统演示 ===" << std::endl;
    
    // 创建RPC服务器
    auto Server = std::make_shared<Helianthus::RPC::RpcServer>();
    
    std::cout << "\n1. 挂载所有反射服务..." << std::endl;
    std::cout << "调试：调用RegisterReflectedServices前，注册表中的服务数量: " << Helianthus::RPC::RpcServiceRegistry::Get().ListServices().size() << std::endl;
    
    Helianthus::RPC::RegisterReflectedServices(*Server);
    std::cout << "调试：调用RegisterReflectedServices后" << std::endl;
    
    std::cout << "\n2. 按标签筛选挂载（只挂载包含'Rpc'标签的服务）..." << std::endl;
    std::vector<std::string> RpcTags = {"Rpc"};
    Helianthus::RPC::RegisterReflectedServices(*Server, RpcTags);
    
    std::cout << "\n3. 按标签筛选挂载（只挂载包含'Admin'标签的服务）..." << std::endl;
    std::vector<std::string> AdminTags = {"Admin"};
    Helianthus::RPC::RegisterReflectedServices(*Server, AdminTags);
    
    std::cout << "\n4. 按多个标签筛选挂载..." << std::endl;
    std::vector<std::string> MultiTags = {"Rpc", "Game"};
    Helianthus::RPC::RegisterReflectedServices(*Server, MultiTags);
    
    std::cout << "\n=== 演示完成 ===" << std::endl;
    return 0;
}