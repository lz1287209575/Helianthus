#include <iostream>
#include <string>
#include <memory>
#include <vector>

#include "Shared/Reflection/Reflection.h"
#include "Shared/Common/Logger.h"

using namespace Helianthus::Reflection;
using namespace Helianthus::Common;

// 使用简化的运行时反射宏定义类
HELIANTHUS_CLASS(HPlayer, HObject)
{
public:
    HPlayer() : Health(100), Speed(5.0f), Name("DefaultPlayer") {}
    
    void TakeDamage(int32_t damage)
    {
        Health -= damage;
        if (Health < 0) Health = 0;
        std::cout << "Player " << Name << " took " << damage << " damage. Health: " << Health << std::endl;
    }
    
    void Heal(int32_t amount)
    {
        Health += amount;
        if (Health > 100) Health = 100;
        std::cout << "Player " << Name << " healed " << amount << " HP. Health: " << Health << std::endl;
    }
    
    std::string GetStatus() const
    {
        return "Player: " + Name + ", Health: " + std::to_string(Health) + ", Speed: " + std::to_string(Speed);
    }
    
    void SetName(const std::string& name)
    {
        Name = name;
        std::cout << "Player name set to: " << Name << std::endl;
    }
    
    // 属性声明
    HELIANTHUS_PROPERTY(Health, int32_t)
    HELIANTHUS_PROPERTY(Speed, float)
    HELIANTHUS_PROPERTY(Name, std::string)
    
    HELIANTHUS_GENERATED_BODY()
};

// 实现运行时反射方法
void* HPlayer::GetProperty(const std::string& PropertyName)
{
    if (PropertyName == "Health") return &Health;
    if (PropertyName == "Speed") return &Speed;
    if (PropertyName == "Name") return &Name;
    return nullptr;
}

void HPlayer::SetProperty(const std::string& PropertyName, void* Value)
{
    if (PropertyName == "Health") Health = *static_cast<int32_t*>(Value);
    if (PropertyName == "Speed") Speed = *static_cast<float*>(Value);
    if (PropertyName == "Name") Name = *static_cast<std::string*>(Value);
}

void* HPlayer::CallFunction(const std::string& FunctionName, const std::vector<void*>& Arguments)
{
    if (FunctionName == "TakeDamage" && Arguments.size() == 1)
    {
        TakeDamage(*static_cast<int32_t*>(Arguments[0]));
    }
    else if (FunctionName == "Heal" && Arguments.size() == 1)
    {
        Heal(*static_cast<int32_t*>(Arguments[0]));
    }
    else if (FunctionName == "SetName" && Arguments.size() == 1)
    {
        SetName(*static_cast<std::string*>(Arguments[0]));
    }
    return nullptr;
}

// 第二个类
HELIANTHUS_CLASS(HWeapon, HObject)
{
public:
    HWeapon() : WeaponName("DefaultWeapon"), Damage(10), Range(100.0f) {}
    
    void Upgrade()
    {
        Damage += 5;
        Range += 10.0f;
        std::cout << "Weapon " << WeaponName << " upgraded! Damage: " << Damage << ", Range: " << Range << std::endl;
    }
    
    std::string GetWeaponInfo() const
    {
        return "Weapon: " + WeaponName + ", Damage: " + std::to_string(Damage) + ", Range: " + std::to_string(Range);
    }
    
    // 属性声明
    HELIANTHUS_PROPERTY(WeaponName, std::string)
    HELIANTHUS_PROPERTY(Damage, int32_t)
    HELIANTHUS_PROPERTY(Range, float)
    
    HELIANTHUS_GENERATED_BODY()
};

// 实现运行时反射方法
void* HWeapon::GetProperty(const std::string& PropertyName)
{
    if (PropertyName == "WeaponName") return &WeaponName;
    if (PropertyName == "Damage") return &Damage;
    if (PropertyName == "Range") return &Range;
    return nullptr;
}

void HWeapon::SetProperty(const std::string& PropertyName, void* Value)
{
    if (PropertyName == "WeaponName") WeaponName = *static_cast<std::string*>(Value);
    if (PropertyName == "Damage") Damage = *static_cast<int32_t*>(Value);
    if (PropertyName == "Range") Range = *static_cast<float*>(Value);
}

void* HWeapon::CallFunction(const std::string& FunctionName, const std::vector<void*>& Arguments)
{
    if (FunctionName == "Upgrade")
    {
        Upgrade();
    }
    return nullptr;
}

// 测试枚举
HELIANTHUS_ENUM(HWeaponType)
{
    SWORD = 0,
    BOW = 1,
    STAFF = 2,
    DAGGER = 3
};

int main()
{
    std::cout << "=== Helianthus 运行时反射系统测试（简化版） ===" << std::endl;
    
    // 初始化反射系统
    InitializeHelianthusReflectionSystem();
    auto& ReflectionSystem = HelianthusReflectionSystem::GetInstance();
    
    // 注册枚举
    HEnumInfo WeaponTypeEnum;
    WeaponTypeEnum.EnumName = "HWeaponType";
    WeaponTypeEnum.EnumValues = {
        {"SWORD", static_cast<int32_t>(HWeaponType::SWORD)},
        {"BOW", static_cast<int32_t>(HWeaponType::BOW)},
        {"STAFF", static_cast<int32_t>(HWeaponType::STAFF)},
        {"DAGGER", static_cast<int32_t>(HWeaponType::DAGGER)}
    };
    ReflectionSystem.RegisterHEnum(WeaponTypeEnum);
    
    std::cout << "\n=== 反射系统注册完成 ===" << std::endl;
    
    // 测试对象操作
    std::cout << "\n=== 测试对象操作 ===" << std::endl;
    
    HPlayer Player;
    HWeapon Weapon;
    
    Player.SetName("TestPlayer");
    Player.TakeDamage(20);
    Player.Heal(10);
    std::cout << "玩家状态: " << Player.GetStatus() << std::endl;
    
    Weapon.WeaponName = "TestSword";
    Weapon.Upgrade();
    std::cout << "武器信息: " << Weapon.GetWeaponInfo() << std::endl;
    
    // 测试运行时反射
    std::cout << "\n=== 测试运行时反射 ===" << std::endl;
    
    // 通过反射访问属性
    int32_t* HealthPtr = static_cast<int32_t*>(Player.GetProperty("Health"));
    if (HealthPtr)
    {
        std::cout << "通过反射获取生命值: " << *HealthPtr << std::endl;
        *HealthPtr = 95;
        std::cout << "通过反射设置生命值后: " << *HealthPtr << std::endl;
    }
    
    // 通过反射调用方法
    std::string NewName = "ReflectedPlayer";
    Player.SetProperty("Name", &NewName);
    std::cout << "通过反射设置名称后: " << Player.Name << std::endl;
    
    int32_t Damage = 15;
    std::vector<void*> Args = {&Damage};
    Player.CallFunction("TakeDamage", Args);
    
    // 测试类型信息
    std::cout << "\n=== 测试类型信息 ===" << std::endl;
    std::cout << "Player 类型: " << Player.GetTypeInfo().name() << std::endl;
    std::cout << "Player 类名: " << Player.GetClassName() << std::endl;
    std::cout << "Weapon 类型: " << Weapon.GetTypeInfo().name() << std::endl;
    std::cout << "Weapon 类名: " << Weapon.GetClassName() << std::endl;
    
    // 反射系统统计
    std::cout << "\n=== 反射系统统计 ===" << std::endl;
    std::cout << "注册的类数量: " << ReflectionSystem.GetRegisteredHClassCount() << std::endl;
    std::cout << "注册的枚举数量: " << ReflectionSystem.GetRegisteredHEnumCount() << std::endl;
    
    auto AllClasses = ReflectionSystem.GetAllHClassInfos();
    std::cout << "\n所有注册的类:" << std::endl;
    for (const auto& Class : AllClasses)
    {
        std::cout << "  - " << Class.ClassName << std::endl;
    }
    
    auto AllEnums = ReflectionSystem.GetAllHEnumInfos();
    std::cout << "\n所有注册的枚举:" << std::endl;
    for (const auto& Enum : AllEnums)
    {
        std::cout << "  - " << Enum.EnumName << std::endl;
    }
    
    std::cout << "\n=== Helianthus 运行时反射系统测试完成 ===" << std::endl;
    
    return 0;
}
