#pragma once

#include "Shared/Reflection/HObject.h"

#include "Player.GEN.h"

// 完全按照您期望的格式
HCLASS(Scriptable)
class Player : public Helianthus::Reflection::HObject
{
public:
    HPROPERTY(ScriptReadable)
    int Level = 1;

    HPROPERTY(ScriptReadable)
    int Exp = 0;

    HPROPERTY(ScriptReadable)
    int Gold = 100;

    HPROPERTY(ScriptReadable)
    int Diamond = 0;

    HFUNCTION(ScriptCallable)
    void OnLevelUp()
    {
        Level++;
        Exp = 0;
        Gold += 100;
    }

    HFUNCTION(ScriptCallable)
    void AddExperience(int amount)
    {
        Exp += amount;
        if (Exp >= Level * 100)
        {
            OnLevelUp();
        }
    }

    HFUNCTION(ScriptCallable)
    std::string GetPlayerInfo() const
    {
        return "Player Level: " + std::to_string(Level) + " Exp: " + std::to_string(Exp) +
               " Gold: " + std::to_string(Gold) + " Diamond: " + std::to_string(Diamond);
    }
};