#include "Shared/Reflection/ReflectionTypes.h"
#include "Shared/Reflection/AutoRegistration.h"
#include "Shared/Reflection/CompileTimeReflection.h"
#include "Shared/Reflection/CodeGenerator.h"
#include "Shared/Common/Logger.h"

#include <iostream>
#include <memory>
#include <string>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <linux/limits.h>
#endif

using namespace Helianthus::Reflection;
using namespace Helianthus::Common;

// 示例类1：使用智能自动注册
class Player
{
public:
    std::string Name;
    int Health;
    float Speed;
    
    Player() : Name("Unknown"), Health(100), Speed(1.0f) {}
    Player(const std::string& name, int health, float speed) 
        : Name(name), Health(health), Speed(speed) {}
    
    void TakeDamage(int damage) { Health -= damage; }
    void Heal(int amount) { Health += amount; }
    bool IsAlive() const { return Health > 0; }
    std::string GetStatus() const { return Name + " (HP: " + std::to_string(Health) + ")"; }
};

// 示例类2：使用编译时反射
class Weapon
{
public:
    std::string Type;
    int Damage;
    float Range;
    
    Weapon() : Type("Sword"), Damage(10), Range(1.5f) {}
    Weapon(const std::string& type, int damage, float range) 
        : Type(type), Damage(damage), Range(range) {}
    
    void Upgrade() { Damage += 5; }
    bool IsRanged() const { return Range > 2.0f; }
    std::string GetDescription() const { return Type + " (DMG: " + std::to_string(Damage) + ")"; }
};

// 示例类3：使用代码生成器
class GameWorld
{
public:
    std::string Name;
    int MaxPlayers;
    bool IsActive;
    
    GameWorld() : Name("Default World"), MaxPlayers(10), IsActive(false) {}
    GameWorld(const std::string& name, int maxPlayers) 
        : Name(name), MaxPlayers(maxPlayers), IsActive(true) {}
    
    void Start() { IsActive = true; }
    void Stop() { IsActive = false; }
    bool CanJoin() const { return IsActive; }
    std::string GetInfo() const { return Name + " (" + std::to_string(MaxPlayers) + " players)"; }
};

int main()
{
    try
    {
        // 初始化Logger
        Helianthus::Common::Logger::Initialize();
        
        Logger::Info("=== 智能反射系统示例 ===");
    
    // 初始化反射系统
    GlobalReflectionSystem = std::make_unique<Helianthus::Reflection::ReflectionSystem>();
    
    Logger::Info("1. 演示智能自动注册");
    
    // 使用智能注册管理器
    auto& SmartManager = SmartRegistrationManager::GetInstance();
    
    // 注册类信息
    SmartManager.RegisterClassInfo("Player", {"Name", "Health", "Speed"}, {"TakeDamage", "Heal", "IsAlive", "GetStatus"});
    SmartManager.RegisterClassInfo("Weapon", {"Type", "Damage", "Range"}, {"Upgrade", "IsRanged", "GetDescription"});
    SmartManager.RegisterClassInfo("GameWorld", {"Name", "MaxPlayers", "IsActive"}, {"Start", "Stop", "CanJoin", "GetInfo"});
    
    Logger::Info("已注册类信息到智能管理器");
    
    // 生成反射代码
    std::string OutputDir = "Generated";
    
    // 创建输出目录
    #ifdef _WIN32
    int result = system(("if not exist " + OutputDir + " mkdir " + OutputDir).c_str());
    #else
    int result = system(("mkdir -p " + OutputDir).c_str());
    #endif
    
    if (result != 0)
    {
        Logger::Error("创建目录失败: " + OutputDir);
    }
    else
    {
        Logger::Info("成功创建目录: " + OutputDir);
    }
    
    if (SmartManager.GenerateAllReflectionCode(OutputDir))
    {
        Logger::Info("成功生成反射代码到目录: " + OutputDir);
    }
    else
    {
        Logger::Error("生成反射代码失败");
    }
    
    Logger::Info("2. 演示编译时反射");
    
    // 使用编译时类型特征
    using PlayerTraits = CompileTime::TypeTraits<Player>;
    Logger::Info("Player类特征:");
    Logger::Info("  - 是类: " + std::string(PlayerTraits::IsClass ? "是" : "否"));
    Logger::Info("  - 是多态: " + std::string(PlayerTraits::IsPolymorphic ? "是" : "否"));
    Logger::Info("  - 可默认构造: " + std::string(PlayerTraits::IsDefaultConstructible ? "是" : "否"));
    Logger::Info("  - 大小: " + std::to_string(PlayerTraits::Size) + " 字节");
    
    using WeaponTraits = CompileTime::TypeTraits<Weapon>;
    Logger::Info("Weapon类特征:");
    Logger::Info("  - 是类: " + std::string(WeaponTraits::IsClass ? "是" : "否"));
    Logger::Info("  - 可复制: " + std::string(WeaponTraits::IsCopyConstructible ? "是" : "否"));
    Logger::Info("  - 可移动: " + std::string(WeaponTraits::IsMoveConstructible ? "是" : "否"));
    
    Logger::Info("3. 演示代码生成器");
    
    // 生成单个类的反射代码
    std::vector<std::string> PlayerProperties = {"Name", "Health", "Speed"};
    std::vector<std::string> PlayerMethods = {"TakeDamage", "Heal", "IsAlive", "GetStatus"};
    
    if (CodeGenerator::GenerateReflectionCode("Player", PlayerProperties, PlayerMethods, "Generated"))
    {
        Logger::Info("成功生成Player类的反射代码");
    }
    
    // 生成Bazel BUILD片段
    std::string BazelFragment = CodeGenerator::GenerateBazelFragment("Player");
    Logger::Info("生成的Bazel BUILD片段:");
    Logger::Info(BazelFragment);
    
    Logger::Info("4. 演示自动注册初始化");
    
    // 使用自动注册初始化器
    AutoRegistrationInitializer::Initialize(GlobalReflectionSystem.get());
    
    Logger::Info("5. 演示实际对象操作");
    
    // 创建对象
    Player player("Hero", 150, 1.2f);
    Weapon weapon("Bow", 25, 3.0f);
    GameWorld world("Fantasy World", 20);
    
    Logger::Info("创建的对象:");
    Logger::Info("  - " + player.GetStatus());
    Logger::Info("  - " + weapon.GetDescription());
    Logger::Info("  - " + world.GetInfo());
    
    // 演示动态操作
    Logger::Info("6. 演示动态属性访问");
    
    // 这里可以添加动态属性访问的代码
    // 由于反射系统已经注册，可以通过反射系统动态访问属性
    
    Logger::Info("=== 智能反射系统示例完成 ===");
    
    // 清理
    AutoRegistrationInitializer::Shutdown();
    GlobalReflectionSystem.reset();
    
    Logger::Info("=== 智能反射系统示例完成 ===");
    return 0;
    }
    catch (const std::exception& e)
    {
        Logger::Error("程序异常: " + std::string(e.what()));
        return 1;
    }
    catch (...)
    {
        Logger::Error("未知异常");
        return 1;
    }
}
