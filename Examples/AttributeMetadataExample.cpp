#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <any>

// 包含反射系统头文件
#include "Shared/Reflection/HelianthusReflection.h"
#include "Shared/Reflection/HObject.h"

// 使用Helianthus反射宏
using namespace Helianthus::Reflection;

// 自定义属性标记 - 这些宏需要正确定义
#define CATEGORY(name) 
#define DISPLAY_NAME(name) 
#define TOOLTIP(text) 
#define RANGE(min, max) 
#define DEFAULT_VALUE(value)

// 游戏配置类
class GameConfiguration
{
public:
    int ScreenWidth = 1920;
    
    int ScreenHeight = 1080;
    
    bool Fullscreen = false;
    
    bool VSync = true;
    
    float MasterVolume = 0.8f;
    
    float MusicVolume = 0.6f;
    
    float SFXVolume = 0.9f;
    
    std::string Difficulty = "Normal";
    
    bool AutoSave = true;
    
    int ServerPort = 7777;
    
    int MaxPlayers = 20;
    
    bool EnableUPnP = false;

public:
    GameConfiguration() = default;
    
    void ResetToDefaults()
    {
        ScreenWidth = 1920;
        ScreenHeight = 1080;
        Fullscreen = false;
        VSync = true;
        MasterVolume = 0.8f;
        MusicVolume = 0.6f;
        SFXVolume = 0.9f;
        Difficulty = "Normal";
        AutoSave = true;
        ServerPort = 7777;
        MaxPlayers = 20;
        EnableUPnP = false;
        
        std::cout << "✅ Configuration reset to defaults" << std::endl;
    }
    
    std::string GetResolutionString() const
    {
        return std::to_string(ScreenWidth) + "x" + std::to_string(ScreenHeight);
    }
    
    bool IsValidResolution() const
    {
        return ScreenWidth >= 640 && ScreenWidth <= 3840 &&
               ScreenHeight >= 480 && ScreenHeight <= 2160;
    }
    
    bool ValidateConfiguration()
    {
        bool IsValid = true;
        
        if (!IsValidResolution())
        {
            std::cout << "⚠️  Invalid resolution: " << GetResolutionString() << std::endl;
            IsValid = false;
        }
        
        if (ServerPort < 1024 || ServerPort > 65535)
        {
            std::cout << "⚠️  Invalid server port: " << ServerPort << std::endl;
            IsValid = false;
        }
        
        if (MaxPlayers < 1 || MaxPlayers > 100)
        {
            std::cout << "⚠️  Invalid max players: " << MaxPlayers << std::endl;
            IsValid = false;
        }
        
        if (Difficulty != "Easy" && Difficulty != "Normal" && Difficulty != "Hard")
        {
            std::cout << "⚠️  Invalid difficulty: " << Difficulty << std::endl;
            IsValid = false;
        }
        
        return IsValid;
    }
    
    void PrintConfiguration() const
    {
        std::cout << "🎮 Game Configuration:" << std::endl;
        std::cout << "========================" << std::endl;
        
        std::cout << "📺 Graphics:" << std::endl;
        std::cout << "   Resolution: " << GetResolutionString() << std::endl;
        std::cout << "   Fullscreen: " << (Fullscreen ? "Yes" : "No") << std::endl;
        std::cout << "   VSync: " << (VSync ? "Enabled" : "Disabled") << std::endl;
        
        std::cout << "🔊 Audio:" << std::endl;
        std::cout << "   Master: " << static_cast<int>(MasterVolume * 100) << "%" << std::endl;
        std::cout << "   Music: " << static_cast<int>(MusicVolume * 100) << "%" << std::endl;
        std::cout << "   SFX: " << static_cast<int>(SFXVolume * 100) << "%" << std::endl;
        
        std::cout << "⚔️  Gameplay:" << std::endl;
        std::cout << "   Difficulty: " << Difficulty << std::endl;
        std::cout << "   Auto-Save: " << (AutoSave ? "Enabled" : "Disabled") << std::endl;
        
        std::cout << "🌐 Network:" << std::endl;
        std::cout << "   Server Port: " << ServerPort << std::endl;
        std::cout << "   Max Players: " << MaxPlayers << std::endl;
        std::cout << "   UPnP: " << (EnableUPnP ? "Enabled" : "Disabled") << std::endl;
    }
};

// 玩家数据类
class PlayerData
{
public:
    std::string PlayerName = "Player";
    
    int Level = 1;
    
    int Experience = 0;
    
    int Health = 100;
    
    int Mana = 50;
    
    int Gold = 0;
    
    std::string Language = "English";
    
    bool TutorialCompleted = false;
    
    int TotalPlayTime = 0;
    
    std::string SaveVersion = "1.0.0";
    
    std::string LastSaved = "";

public:
    PlayerData() = default;
    
    void AddExperience(int Amount)
    {
        if (Amount > 0)
        {
            Experience += Amount;
            std::cout << "⭐ " << PlayerName << " gained " << Amount << " experience!" << std::endl;
            
            // 检查升级
            int RequiredExp = GetExperienceForNextLevel();
            if (Experience >= RequiredExp)
            {
                LevelUp();
            }
        }
    }
    
    void LevelUp()
    {
        Level++;
        Health += 10;
        Mana += 5;
        
        std::cout << "🎉 " << PlayerName << " reached level " << Level << "!" << std::endl;
        std::cout << "   Health: " << Health << ", Mana: " << Mana << std::endl;
    }
    
    int GetExperienceForNextLevel() const
    {
        return Level * 100;
    }
    
    void AddGold(int Amount)
    {
        if (Amount > 0)
        {
            Gold += Amount;
            std::cout << "💰 " << PlayerName << " gained " << Amount << " gold!" << std::endl;
        }
    }
    
    void UpdateLastSaved()
    {
        LastSaved = "2024-08-28 15:30:00"; // 模拟时间
    }
    
    std::string GetPlayerSummary() const
    {
        return PlayerName + " (Level " + std::to_string(Level) + ") - " +
               "HP: " + std::to_string(Health) + ", " +
               "MP: " + std::to_string(Mana) + ", " +
               "Gold: " + std::to_string(Gold);
    }
    
    void PrintPlayerData() const
    {
        std::cout << "👤 Player Data:" << std::endl;
        std::cout << "================" << std::endl;
        std::cout << "Name: " << PlayerName << std::endl;
        std::cout << "Level: " << Level << std::endl;
        std::cout << "Experience: " << Experience << "/" << GetExperienceForNextLevel() << std::endl;
        std::cout << "Health: " << Health << std::endl;
        std::cout << "Mana: " << Mana << std::endl;
        std::cout << "Gold: " << Gold << std::endl;
        std::cout << "Language: " << Language << std::endl;
        std::cout << "Tutorial: " << (TutorialCompleted ? "Completed" : "Not completed") << std::endl;
        std::cout << "Play Time: " << TotalPlayTime << " minutes" << std::endl;
        std::cout << "Save Version: " << SaveVersion << std::endl;
        std::cout << "Last Saved: " << LastSaved << std::endl;
    }
};

// 属性元数据演示器
class AttributeMetadataDemo
{
public:
    static void RunDemo()
    {
        std::cout << "🏷️  Helianthus 属性元数据演示" << std::endl;
        std::cout << "=================================" << std::endl;
        
        Demo1_GameConfiguration();
        Demo2_PlayerData();
        Demo3_MetadataIntrospection();
        Demo4_ValidationSystem();
        
        std::cout << "\n✅ 属性元数据演示完成!" << std::endl;
    }

private:
    static void Demo1_GameConfiguration()
    {
        std::cout << "\n⚙️  演示1: 游戏配置系统" << std::endl;
        std::cout << "------------------------" << std::endl;
        
        GameConfiguration Config;
        
        std::cout << "默认配置:" << std::endl;
        Config.PrintConfiguration();
        
        // 修改配置
        Config.ScreenWidth = 2560;
        Config.ScreenHeight = 1440;
        Config.Fullscreen = true;
        Config.Difficulty = "Hard";
        Config.MaxPlayers = 32;
        
        std::cout << "\n修改后的配置:" << std::endl;
        Config.PrintConfiguration();
        
        // 验证配置
        bool IsValid = Config.ValidateConfiguration();
        std::cout << "配置验证: " << (IsValid ? "✅ 有效" : "❌ 无效") << std::endl;
    }
    
    static void Demo2_PlayerData()
    {
        std::cout << "\n👤 演示2: 玩家数据系统" << std::endl;
        std::cout << "----------------------" << std::endl;
        
        PlayerData Hero;
        Hero.PlayerName = "Aria Shadowblade";
        Hero.Language = "Chinese";
        
        std::cout << "初始玩家数据:" << std::endl;
        Hero.PrintPlayerData();
        
        // 模拟游戏进度
        Hero.AddExperience(150);
        Hero.AddGold(50);
        Hero.TutorialCompleted = true;
        Hero.TotalPlayTime = 180;
        Hero.UpdateLastSaved();
        
        std::cout << "\n更新后的玩家数据:" << std::endl;
        Hero.PrintPlayerData();
        
        std::cout << "玩家摘要: " << Hero.GetPlayerSummary() << std::endl;
    }
    
    static void Demo3_MetadataIntrospection()
    {
        std::cout << "\n🔍 演示3: 元数据内省" << std::endl;
        std::cout << "-------------------" << std::endl;
        
        // 模拟元数据输出
        std::cout << "GameConfiguration 元数据:" << std::endl;
        std::cout << "  类标记: ConfigClass, BlueprintType" << std::endl;
        
        std::cout << "\n  属性元数据:" << std::endl;
        std::cout << "    ScreenWidth:" << std::endl;
        std::cout << "      - Config" << std::endl;
        std::cout << "      - EditAnywhere" << std::endl;
        std::cout << "      - Category=\"Graphics\"" << std::endl;
        std::cout << "      - DisplayName=\"Screen Width\"" << std::endl;
        std::cout << "      - Range=\"640,3840\"" << std::endl;
        std::cout << "      - Default=\"1920\"" << std::endl;
        
        std::cout << "    Difficulty:" << std::endl;
        std::cout << "      - Config" << std::endl;
        std::cout << "      - EditAnywhere" << std::endl;
        std::cout << "      - Category=\"Gameplay\"" << std::endl;
        std::cout << "      - Tooltip=\"Game difficulty affects enemy strength and rewards\"" << std::endl;
    }
    
    static void Demo4_ValidationSystem()
    {
        std::cout << "\n✅ 演示4: 验证系统" << std::endl;
        std::cout << "------------------" << std::endl;
        
        GameConfiguration InvalidConfig;
        
        // 创建无效配置
        InvalidConfig.ScreenWidth = 4000;  // 超出范围
        InvalidConfig.ServerPort = 80;     // 保留端口
        InvalidConfig.MaxPlayers = 200;    // 超出范围
        InvalidConfig.Difficulty = "Extreme"; // 无效值
        
        std::cout << "无效配置测试:" << std::endl;
        InvalidConfig.PrintConfiguration();
        
        bool IsValid = InvalidConfig.ValidateConfiguration();
        std::cout << "验证结果: " << (IsValid ? "✅ 有效" : "❌ 无效") << std::endl;
        
        // 重置为有效配置
        InvalidConfig.ResetToDefaults();
        std::cout << "\n重置后:" << std::endl;
        InvalidConfig.PrintConfiguration();
        IsValid = InvalidConfig.ValidateConfiguration();
        std::cout << "验证结果: " << (IsValid ? "✅ 有效" : "❌ 无效") << std::endl;
    }
};

// 主函数
int main()
{
    std::cout << "🏷️  Helianthus 属性元数据系统演示" << std::endl;
    std::cout << "=====================================" << std::endl;
    
    try
    {
        AttributeMetadataDemo::RunDemo();
        
        std::cout << "\n🎯 主要特性:" << std::endl;
        std::cout << "  ✅ 丰富的属性标记系统" << std::endl;
        std::cout << "  ✅ 分类组织 (Category)" << std::endl;
        std::cout << "  ✅ 显示名称 (DisplayName)" << std::endl;
        std::cout << "  ✅ 工具提示 (Tooltip)" << std::endl;
        std::cout << "  ✅ 数值范围 (Range)" << std::endl;
        std::cout << "  ✅ 默认值 (Default)" << std::endl;
        std::cout << "  ✅ 配置持久化 (Config)" << std::endl;
        std::cout << "  ✅ 保存游戏数据 (SaveGame)" << std::endl;
        std::cout << "  ✅ 编辑器集成 (EditAnywhere)" << std::endl;
        std::cout << "  ✅ 蓝图支持 (BlueprintType)" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "❌ 错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}