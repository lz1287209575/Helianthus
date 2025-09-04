#pragma once

#include "Shared/Reflection/HClassPascal.h"

#include "Player.GEN.h"

// 完全符合PascalCase命名规范的示例
HCLASS(Scriptable | BlueprintType | Category = "Player")
class Player : public Helianthus::Reflection::HObject
{
public:
    HPROPERTY(ScriptReadable | BlueprintReadOnly | Category = "Stats" |
                                                              DisplayName = "Player Level")
    int Level = 1;

    HPROPERTY(ScriptReadable | BlueprintReadWrite | Category = "Stats" | SaveGame)
    int Experience = 0;

    HPROPERTY(SaveGame | Config | Category = "Economy" | DefaultValue = "100")
    int Gold = 100;

    HPROPERTY(SaveGame | BlueprintReadWrite | Category = "Economy")
    int Diamond = 0;

    HPROPERTY(Config | Category = "Settings" | VisibleAnywhere)
    std::string PlayerName = "NewPlayer";

    HFUNCTION(ScriptCallable | Category = "Leveling")
    void OnLevelUp()
    {
        Level++;
        Experience = 0;
        Gold += 50;
    }

    HFUNCTION(BlueprintCallable | Category = "Economy")
    void AddGold(int Amount)
    {
        Gold += Amount;
    }

    HFUNCTION(BlueprintCallable | Category = "Economy" | BlueprintPure)
    int GetTotalWealth() const
    {
        return Gold + Diamond * 100;
    }

    HFUNCTION(ScriptCallable | Category = "Stats")
    std::string GetPlayerInfo() const
    {
        return "Player: " + PlayerName + " Level: " + std::to_string(Level) +
               " Gold: " + std::to_string(Gold) + " Diamond: " + std::to_string(Diamond);
    }
};

// 使用示例
HCLASS(Scriptable | BlueprintType)
class GameManager : public Helianthus::Reflection::HObject
{
public:
    HPROPERTY(Config | Category = "GameSettings")
    int MaxPlayers = 100;

    HPROPERTY(Config | Category = "GameSettings")
    int ServerPort = 8080;

    HPROPERTY(BlueprintReadOnly | Category = "Runtime")
    int ActivePlayers = 0;

    HFUNCTION(BlueprintCallable | NetServer | AuthorityOnly)
    bool StartServer(int Port)
    {
        ServerPort = Port;
        return true;
    }

    HFUNCTION(BlueprintCallable | NetMulticast)
    void BroadcastMessage(const std::string& Message)
    {
        // 广播消息给所有客户端
    }
};