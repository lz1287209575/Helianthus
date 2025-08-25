#include "Shared/Reflection/HelianthusReflection.h"
#include "Shared/Reflection/HelianthusCodeGenerator.h"
#include "Shared/Common/Logger.h"

#include <iostream>
#include <memory>
#include <string>

using namespace Helianthus::Reflection;
using namespace Helianthus::Common;

// 简化的Helianthus风格基础类
class HelianthusBaseObject : public HelianthusObject
{
public:
    std::string Name;
    int ID;
    
    // 构造函数
    HelianthusBaseObject() : Name("Unknown"), ID(0) {}
    HelianthusBaseObject(const std::string& name, int id) : Name(name), ID(id) {}
    
    // 方法实现
    std::string GetName() const { return Name; }
    void SetName(const std::string& name) { Name = name; }
    
    // 反射接口实现
    virtual const HelianthusClassInfo* GetClass() const override { 
        return StaticClass();
    }
    
    static HelianthusClassInfo* StaticClass() {
        static HelianthusClassInfo* ClassInfo = nullptr;
        if (!ClassInfo) {
            ClassInfo = new HelianthusClassInfo();
            ClassInfo->Name = "HelianthusBaseObject";
            ClassInfo->FullName = "HelianthusBaseObject";
            ClassInfo->TypeIndex = std::type_index(typeid(HelianthusBaseObject));
        }
        return ClassInfo;
    }
    virtual const std::string& GetClassName() const override { 
        static const std::string ClassName = "HelianthusBaseObject"; 
        return ClassName; 
    }
};

// 简化的Helianthus风格玩家类
class HelianthusPlayer : public HelianthusBaseObject
{
public:
    int Health;
    float Speed;
    bool IsAlive;
    
    // 构造函数
    HelianthusPlayer() : Health(100), Speed(1.0f), IsAlive(true) {}
    HelianthusPlayer(const std::string& name, int health, float speed) 
        : HelianthusBaseObject(name, 0), Health(health), Speed(speed), IsAlive(true) {}
    
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
    
    // 反射接口实现
    virtual const HelianthusClassInfo* GetClass() const override { 
        return StaticClass();
    }
    
    static HelianthusClassInfo* StaticClass() {
        static HelianthusClassInfo* ClassInfo = nullptr;
        if (!ClassInfo) {
            ClassInfo = new HelianthusClassInfo();
            ClassInfo->Name = "HelianthusPlayer";
            ClassInfo->FullName = "HelianthusPlayer";
            ClassInfo->TypeIndex = std::type_index(typeid(HelianthusPlayer));
            ClassInfo->SuperClass = HelianthusBaseObject::StaticClass();
        }
        return ClassInfo;
    }
    virtual const std::string& GetClassName() const override { 
        static const std::string ClassName = "HelianthusPlayer"; 
        return ClassName; 
    }
};

// 简化的Helianthus风格武器类
class HelianthusWeapon : public HelianthusBaseObject
{
public:
    int Damage;
    float Range;
    std::string WeaponType;
    
    // 构造函数
    HelianthusWeapon() : Damage(10), Range(1.5f), WeaponType("Sword") {}
    HelianthusWeapon(const std::string& name, int damage, float range, const std::string& type)
        : HelianthusBaseObject(name, 0), Damage(damage), Range(range), WeaponType(type) {}
    
    // 方法实现
    void Upgrade() { Damage += 5; }
    bool IsRanged() const { return Range > 2.0f; }
    std::string GetDescription() const 
    { 
        return Name + " (" + WeaponType + ", DMG: " + std::to_string(Damage) + ")"; 
    }
    
    // 反射接口实现
    virtual const HelianthusClassInfo* GetClass() const override { 
        return StaticClass();
    }
    
    static HelianthusClassInfo* StaticClass() {
        static HelianthusClassInfo* ClassInfo = nullptr;
        if (!ClassInfo) {
            ClassInfo = new HelianthusClassInfo();
            ClassInfo->Name = "HelianthusWeapon";
            ClassInfo->FullName = "HelianthusWeapon";
            ClassInfo->TypeIndex = std::type_index(typeid(HelianthusWeapon));
            ClassInfo->SuperClass = HelianthusBaseObject::StaticClass();
        }
        return ClassInfo;
    }
    virtual const std::string& GetClassName() const override { 
        static const std::string ClassName = "HelianthusWeapon"; 
        return ClassName; 
    }
};

int main()
{
    try
    {
        // 初始化Logger
        Helianthus::Common::Logger::Initialize();
        
        Logger::Info("=== Helianthus风格反射系统示例 ===");
        
        Logger::Info("1. 演示Helianthus风格类注册");
        
        // 使用Helianthus风格注册管理器
        auto& HelianthusManager = HelianthusReflectionManager::GetInstance();
        
        // 注册Helianthus风格类
        HelianthusManager.RegisterHelianthusClass("HelianthusBaseObject", "HelianthusObject", {"Name", "ID"}, {"GetName", "SetName"});
        HelianthusManager.RegisterHelianthusClass("HelianthusPlayer", "HelianthusBaseObject", {"Health", "Speed", "IsAlive"}, {"TakeDamage", "Heal", "IsPlayerAlive"});
        HelianthusManager.RegisterHelianthusClass("HelianthusWeapon", "HelianthusBaseObject", {"Damage", "Range", "WeaponType"}, {"Upgrade", "IsRanged", "GetDescription"});
        
        Logger::Info("已注册Helianthus风格类到管理器");
        
        Logger::Info("2. 演示Helianthus风格反射代码生成");
        
        // 生成Helianthus风格反射代码
        std::string OutputDir = "Generated";
        if (HelianthusManager.GenerateAllHelianthusReflectionCode(OutputDir))
        {
            Logger::Info("成功生成Helianthus风格反射代码到目录: " + OutputDir);
        }
        else
        {
            Logger::Error("生成Helianthus风格反射代码失败");
        }
        
        Logger::Info("3. 演示Helianthus风格对象创建和操作");
        
        // 创建Helianthus风格对象
        auto* Player = new HelianthusPlayer("Hero", 150, 1.2f);
        auto* Weapon = new HelianthusWeapon("MagicSword", 25, 2.5f, "Sword");
        
        if (Player && Weapon)
        {
            Logger::Info("成功创建Helianthus风格对象");
            
            // 演示基本操作
            Logger::Info("Player信息:");
            Logger::Info("  - Name: " + Player->GetName());
            Logger::Info("  - Health: " + std::to_string(Player->Health));
            Logger::Info("  - Speed: " + std::to_string(Player->Speed));
            
            Logger::Info("Weapon信息:");
            Logger::Info("  - Name: " + Weapon->GetName());
            Logger::Info("  - Damage: " + std::to_string(Weapon->Damage));
            Logger::Info("  - Range: " + std::to_string(Weapon->Range));
            Logger::Info("  - Type: " + Weapon->WeaponType);
            
            // 演示方法调用
            Player->TakeDamage(30);
            Logger::Info("Player受到30点伤害，剩余血量: " + std::to_string(Player->Health));
            
            Player->Heal(20);
            Logger::Info("Player恢复20点血量，当前血量: " + std::to_string(Player->Health));
            
            Weapon->Upgrade();
            Logger::Info("Weapon升级后伤害: " + std::to_string(Weapon->Damage));
            
            // 演示继承关系
            Logger::Info("4. 演示Helianthus风格继承关系");
            
            if (Player->IsA<HelianthusBaseObject>())
            {
                Logger::Info("Player是HelianthusBaseObject的实例");
            }
            
            if (Weapon->IsA<HelianthusBaseObject>())
            {
                Logger::Info("Weapon是HelianthusBaseObject的实例");
            }
            
            // 演示动态转换
            if (auto* BasePlayer = Player->Cast<HelianthusBaseObject>())
            {
                Logger::Info("成功将Player转换为HelianthusBaseObject");
                Logger::Info("  - Base Name: " + BasePlayer->GetName());
            }
            
            // 演示反射信息查询
            Logger::Info("5. 演示Helianthus风格反射信息查询");
            
            Logger::Info("Player类信息:");
            Logger::Info("  - 类名: " + Player->GetClassName());
            
            Logger::Info("Weapon类信息:");
            Logger::Info("  - 类名: " + Weapon->GetClassName());
            
            // 清理对象
            delete Player;
            delete Weapon;
        }
        
        Logger::Info("6. 演示Helianthus风格宏生成");
        
        // 生成Helianthus风格宏
        std::string HelianthusMacros = HelianthusCodeGenerator::GenerateHelianthusMacros("HelianthusPlayer");
        Logger::Info("生成的Helianthus风格宏:");
        Logger::Info(HelianthusMacros);
        
        // 生成Helianthus风格构建配置
        std::string HelianthusBuildConfig = HelianthusCodeGenerator::GenerateHelianthusBuildConfig("HelianthusPlayer");
        Logger::Info("生成的Helianthus风格构建配置:");
        Logger::Info(HelianthusBuildConfig);
        
        Logger::Info("=== Helianthus风格反射系统示例完成 ===");
        
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
