#include "Shared/Reflection/UE_Style_Reflection.h"
#include "Shared/Reflection/UE_Style_CodeGenerator.h"
#include "Shared/Common/Logger.h"

#include <iostream>
#include <memory>
#include <string>

using namespace Helianthus::Reflection;
using namespace Helianthus::Common;

// UE风格的基础类
UCLASS(UBaseObject, UObject)
{
public:
    UPROPERTY(Name, std::string);
    UPROPERTY(ID, int);
    
    UFUNCTION(GetName, std::string);
    UFUNCTION(SetName, void, const std::string&);
    
    // 构造函数
    UBaseObject() : Name("Unknown"), ID(0) {}
    UBaseObject(const std::string& name, int id) : Name(name), ID(id) {}
    
    // 方法实现
    std::string GetName() const { return Name; }
    void SetName(const std::string& name) { Name = name; }
};

// UE风格的玩家类
UCLASS(UPlayer, UBaseObject)
{
public:
    UPROPERTY(Health, int);
    UPROPERTY(Speed, float);
    UPROPERTY(IsAlive, bool);
    
    UFUNCTION(TakeDamage, void, int);
    UFUNCTION(Heal, void, int);
    UFUNCTION(IsPlayerAlive, bool);
    
    // 构造函数
    UPlayer() : Health(100), Speed(1.0f), IsAlive(true) {}
    UPlayer(const std::string& name, int health, float speed) 
        : UBaseObject(name, 0), Health(health), Speed(speed), IsAlive(true) {}
    
    // 方法实现
    void TakeDamage(int damage) 
    { 
        Health -= damage; 
        if (Health <= 0) 
        {
            Health = 0;
            IsAlive = false;
        }
    }
    
    void Heal(int amount) 
    { 
        Health += amount; 
        if (Health > 0) IsAlive = true;
    }
    
    bool IsPlayerAlive() const { return IsAlive; }
};

// UE风格的武器类
UCLASS(UWeapon, UBaseObject)
{
public:
    UPROPERTY(Damage, int);
    UPROPERTY(Range, float);
    UPROPERTY(WeaponType, std::string);
    
    UFUNCTION(Upgrade, void);
    UFUNCTION(IsRanged, bool);
    UFUNCTION(GetDescription, std::string);
    
    // 构造函数
    UWeapon() : Damage(10), Range(1.5f), WeaponType("Sword") {}
    UWeapon(const std::string& name, int damage, float range, const std::string& type)
        : UBaseObject(name, 0), Damage(damage), Range(range), WeaponType(type) {}
    
    // 方法实现
    void Upgrade() { Damage += 5; }
    bool IsRanged() const { return Range > 2.0f; }
    std::string GetDescription() const 
    { 
        return Name + " (" + WeaponType + ", DMG: " + std::to_string(Damage) + ")"; 
    }
};

int main()
{
    try
    {
        // 初始化Logger
        Helianthus::Common::Logger::Initialize();
        
        Logger::Info("=== UE风格反射系统示例 ===");
        
        Logger::Info("1. 演示UE风格类注册");
        
        // 使用UE风格注册管理器
        auto& UEManager = UEReflectionManager::GetInstance();
        
        // 注册UE风格类
        UEManager.RegisterUEClass("UBaseObject", "UObject", {"Name", "ID"}, {"GetName", "SetName"});
        UEManager.RegisterUEClass("UPlayer", "UBaseObject", {"Health", "Speed", "IsAlive"}, {"TakeDamage", "Heal", "IsPlayerAlive"});
        UEManager.RegisterUEClass("UWeapon", "UBaseObject", {"Damage", "Range", "WeaponType"}, {"Upgrade", "IsRanged", "GetDescription"});
        
        Logger::Info("已注册UE风格类到管理器");
        
        Logger::Info("2. 演示UE风格代码生成");
        
        // 生成UE风格反射代码
        std::string OutputDir = "./Generated_UE";
        
        // 创建输出目录
        #ifdef _WIN32
        int result = system(("if not exist \"" + OutputDir + "\" mkdir \"" + OutputDir + "\"").c_str());
        #else
        int result = system(("mkdir -p \"" + OutputDir + "\"").c_str());
        #endif
        
        if (result == 0)
        {
            Logger::Info("成功创建UE代码生成目录: " + OutputDir);
        }
        
        if (UEManager.GenerateAllUEReflectionCode(OutputDir))
        {
            Logger::Info("成功生成UE风格反射代码到目录: " + OutputDir);
        }
        else
        {
            Logger::Error("生成UE风格反射代码失败");
        }
        
        Logger::Info("3. 演示UE风格对象创建和操作");
        
        // 创建UE风格对象
        auto* Player = UReflectionSystem::Get().CreateObject<UPlayer>();
        auto* Weapon = UReflectionSystem::Get().CreateObject<UWeapon>();
        
        if (Player && Weapon)
        {
            Logger::Info("成功创建UE风格对象");
            
            // 使用类型安全的属性访问
            auto PlayerName = PROPERTY(Player, Name, std::string);
            auto PlayerHealth = PROPERTY(Player, Health, int);
            auto WeaponDamage = PROPERTY(Weapon, Damage, int);
            
            // 设置属性
            PlayerName = "Hero";
            PlayerHealth = 150;
            WeaponDamage = 25;
            
            Logger::Info("设置属性:");
            Logger::Info("  - Player.Name = " + PlayerName.Get());
            Logger::Info("  - Player.Health = " + std::to_string(PlayerHealth.Get()));
            Logger::Info("  - Weapon.Damage = " + std::to_string(WeaponDamage.Get()));
            
            // 使用类型安全的函数调用
            auto TakeDamageFunc = FUNCTION(Player, TakeDamage, void, int);
            auto HealFunc = FUNCTION(Player, Heal, void, int);
            auto UpgradeFunc = FUNCTION(Weapon, Upgrade, void);
            
            // 调用方法
            TakeDamageFunc(30);
            Logger::Info("Player受到30点伤害，剩余血量: " + std::to_string(PlayerHealth.Get()));
            
            HealFunc(20);
            Logger::Info("Player恢复20点血量，当前血量: " + std::to_string(PlayerHealth.Get()));
            
            UpgradeFunc();
            Logger::Info("Weapon升级后伤害: " + std::to_string(WeaponDamage.Get()));
            
            // 演示继承关系
            Logger::Info("4. 演示UE风格继承关系");
            
            if (Player->IsA<UBaseObject>())
            {
                Logger::Info("Player是UBaseObject的实例");
            }
            
            if (Weapon->IsA<UBaseObject>())
            {
                Logger::Info("Weapon是UBaseObject的实例");
            }
            
            // 演示动态转换
            if (auto* BasePlayer = Player->Cast<UBaseObject>())
            {
                Logger::Info("成功将Player转换为UBaseObject");
            }
            
            // 演示反射信息查询
            Logger::Info("5. 演示UE风格反射信息查询");
            
            auto* PlayerClass = Player->GetClass();
            if (PlayerClass)
            {
                Logger::Info("Player类信息:");
                Logger::Info("  - 类名: " + PlayerClass->Name);
                Logger::Info("  - 属性数量: " + std::to_string(PlayerClass->GetAllProperties().size()));
                Logger::Info("  - 方法数量: " + std::to_string(PlayerClass->GetAllMethods().size()));
                
                if (PlayerClass->SuperClass)
                {
                    Logger::Info("  - 父类: " + PlayerClass->SuperClass->Name);
                }
            }
            
            // 清理对象
            UReflectionSystem::Get().DestroyObject(Player);
            UReflectionSystem::Get().DestroyObject(Weapon);
        }
        
        Logger::Info("6. 演示UE风格宏生成");
        
        // 生成UE风格宏
        std::string UEMacros = UECodeGenerator::GenerateUEMacros("UPlayer");
        Logger::Info("生成的UE风格宏:");
        Logger::Info(UEMacros);
        
        // 生成UE风格构建配置
        std::string UEBuildConfig = UECodeGenerator::GenerateUEBuildConfig("UPlayer");
        Logger::Info("生成的UE风格构建配置:");
        Logger::Info(UEBuildConfig);
        
        Logger::Info("=== UE风格反射系统示例完成 ===");
        
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
