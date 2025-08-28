#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <any>

// 包含反射系统头文件
#include "Shared/Reflection/HelianthusReflection.h"
#include "Shared/Reflection/HObject.h"

// 使用Helianthus反射宏
using namespace Helianthus::Reflection;

// 基础游戏对象类
class GameObject : public HObject
{
public:
    GameObject() : ObjectId(NextObjectId++) {}
    virtual ~GameObject() = default;
    
    int GetObjectId() const { return ObjectId; }
    
private:
    static int NextObjectId;
    int ObjectId;
};

int GameObject::NextObjectId = 1000;

// 游戏实体类 - 使用HCLASS宏定义
HCLASS(Scriptable | BlueprintType | Config)
class Entity : public GameObject
{
public:
    HPROPERTY(ScriptReadable | BlueprintReadWrite | Category="Stats" | DisplayName="Health Points" | Tooltip="Current health of the entity")
    int Health = 100;
    
    HPROPERTY(ScriptReadable | BlueprintReadWrite | Category="Stats" | DisplayName="Maximum Health")
    int MaxHealth = 100;
    
    HPROPERTY(ScriptReadable | BlueprintReadOnly | Category="Identity" | SaveGame)
    std::string Name = "Unnamed Entity";
    
    HPROPERTY(Config | EditAnywhere | Category="Physics" | Tooltip="Movement speed in units per second")
    float MovementSpeed = 5.0f;
    
    HPROPERTY(SaveGame | BlueprintReadWrite | Category="State")
    bool IsActive = true;
    
    HPROPERTY(BlueprintReadOnly | Category="Transform")
    float PositionX = 0.0f;
    
    HPROPERTY(BlueprintReadOnly | Category="Transform")
    float PositionY = 0.0f;
    
    HPROPERTY(BlueprintReadOnly | Category="Transform")
    float PositionZ = 0.0f;

public:
    Entity() = default;
    
    HFUNCTION(ScriptCallable | BlueprintCallable | Category="Combat")
    void TakeDamage(int DamageAmount)
    {
        if (DamageAmount > 0 && IsActive)
        {
            Health = std::max(0, Health - DamageAmount);
            if (Health <= 0)
            {
                OnDeath();
            }
        }
    }
    
    HFUNCTION(ScriptCallable | BlueprintCallable | Category="Combat")
    void Heal(int HealAmount)
    {
        if (HealAmount > 0 && IsActive)
        {
            Health = std::min(MaxHealth, Health + HealAmount);
        }
    }
    
    HFUNCTION(BlueprintPure | Category="Combat")
    float GetHealthPercentage() const
    {
        return MaxHealth > 0 ? static_cast<float>(Health) / MaxHealth : 0.0f;
    }
    
    HFUNCTION(BlueprintCallable | Category="Movement")
    void Move(float DeltaX, float DeltaY, float DeltaZ)
    {
        if (IsActive)
        {
            PositionX += DeltaX * MovementSpeed;
            PositionY += DeltaY * MovementSpeed;
            PositionZ += DeltaZ * MovementSpeed;
            
            std::cout << Name << " moved to (" << PositionX << ", " << PositionY << ", " << PositionZ << ")" << std::endl;
        }
    }
    
    HFUNCTION(ScriptEvent | BlueprintEvent | Category="Lifecycle")
    void OnDeath()
    {
        IsActive = false;
        std::cout << Name << " has died!" << std::endl;
    }
    
    HFUNCTION(BlueprintCallable | Category="Utility")
    std::string GetDebugInfo() const
    {
        return Name + " [ID:" + std::to_string(GetObjectId()) + "] " +
               "HP:" + std::to_string(Health) + "/" + std::to_string(MaxHealth) + " " +
               "POS:(" + std::to_string(PositionX) + "," + std::to_string(PositionY) + "," + std::to_string(PositionZ) + ")";
    }
};

// 玩家类 - 继承自Entity
HCLASS(Scriptable | BlueprintType | ConfigClass | DefaultConfig)
class Player : public Entity
{
public:
    HPROPERTY(ScriptReadable | BlueprintReadWrite | Category="Progress" | SaveGame)
    int Level = 1;
    
    HPROPERTY(ScriptReadable | BlueprintReadWrite | Category="Progress" | SaveGame)
    int Experience = 0;
    
    HPROPERTY(Config | EditAnywhere | Category="Player" | Tooltip="Player's chosen class")
    std::string PlayerClass = "Adventurer";
    
    HPROPERTY(SaveGame | BlueprintReadWrite | Category="Economy")
    int Gold = 0;
    
    HPROPERTY(BlueprintReadOnly | Category="Progress")
    int ExperienceToNextLevel = 100;
    
    HPROPERTY(BlueprintReadOnly | Category="Combat")
    int AttackPower = 10;

public:
    Player()
    {
        Name = "Player";
        MaxHealth = 150;
        Health = MaxHealth;
        UpdateStats();
    }
    
    HFUNCTION(ScriptCallable | BlueprintCallable | Category="Progress")
    void AddExperience(int ExpAmount)
    {
        if (ExpAmount <= 0) return;
        
        Experience += ExpAmount;
        std::cout << Name << " gained " << ExpAmount << " experience!" << std::endl;
        
        while (Experience >= ExperienceToNextLevel)
        {
            Experience -= ExperienceToNextLevel;
            LevelUp();
        }
    }
    
    HFUNCTION(ScriptCallable | BlueprintCallable | Category="Progress")
    void LevelUp()
    {
        Level++;
        MaxHealth += 20;
        Health = MaxHealth; // Full heal on level up
        AttackPower += 5;
        ExperienceToNextLevel = static_cast<int>(ExperienceToNextLevel * 1.5f);
        
        std::cout << "🎉 " << Name << " reached level " << Level << "!" << std::endl;
        std::cout << "   Health increased to " << MaxHealth << std::endl;
        std::cout << "   Attack power increased to " << AttackPower << std::endl;
        
        OnLevelUp();
    }
    
    HFUNCTION(BlueprintPure | Category="Combat")
    int GetTotalAttackPower() const
    {
        return AttackPower + Level * 2;
    }
    
    HFUNCTION(ScriptCallable | BlueprintCallable | Category="Combat")
    void Attack(Entity* Target)
    {
        if (Target && Target->IsActive)
        {
            int Damage = GetTotalAttackPower();
            std::cout << "⚔️  " << Name << " attacks " << Target->Name << " for " << Damage << " damage!" << std::endl;
            Target->TakeDamage(Damage);
        }
    }
    
    HFUNCTION(BlueprintCallable | Category="Economy")
    void AddGold(int Amount)
    {
        if (Amount > 0)
        {
            Gold += Amount;
            std::cout << "💰 " << Name << " gained " << Amount << " gold!" << std::endl;
        }
    }
    
    HFUNCTION(BlueprintEvent | Category="Events")
    void OnLevelUp()
    {
        std::cout << "🌟 " << Name << " feels stronger!" << std::endl;
    }
    
private:
    void UpdateStats()
    {
        AttackPower = 10 + (Level - 1) * 5;
        ExperienceToNextLevel = 100 + (Level - 1) * 50;
    }
};

// 敌人类
HCLASS(Scriptable | BlueprintType)
class Enemy : public Entity
{
public:
    HPROPERTY(BlueprintReadOnly | Category="Combat")
    int BaseAttackPower = 8;
    
    HPROPERTY(BlueprintReadOnly | Category="Rewards")
    int ExperienceReward = 25;
    
    HPROPERTY(BlueprintReadOnly | Category="Rewards")
    int GoldReward = 15;
    
    HPROPERTY(BlueprintReadOnly | Category="Combat")
    std::string EnemyType = "Monster";

public:
    Enemy()
    {
        Name = "Enemy";
        MaxHealth = 50;
        Health = MaxHealth;
    }
    
    HFUNCTION(BlueprintCallable | Category="Setup")
    void ConfigureEnemy(const std::string& Type, int Tier)
    {
        EnemyType = Type;
        Name = Type + " Lv" + std::to_string(Tier);
        
        // 根据等级调整属性
        BaseAttackPower = 8 + Tier * 3;
        MaxHealth = 50 + Tier * 15;
        Health = MaxHealth;
        ExperienceReward = 25 + Tier * 10;
        GoldReward = 15 + Tier * 5;
        MovementSpeed = 3.0f + Tier * 0.5f;
    }
    
    HFUNCTION(ScriptCallable | BlueprintCallable | Category="Combat")
    void PerformAttack(Player* Target)
    {
        if (Target && Target->IsActive)
        {
            std::cout << "👹 " << Name << " attacks " << Target->Name << " for " << BaseAttackPower << " damage!" << std::endl;
            Target->TakeDamage(BaseAttackPower);
        }
    }
    
    HFUNCTION(BlueprintPure | Category="Info")
    std::string GetEnemyInfo() const
    {
        return Name + " [" + EnemyType + "] - HP: " + std::to_string(Health) + "/" + std::to_string(MaxHealth);
    }
};

// 物品类
HCLASS(Scriptable | BlueprintType | Config)
class Item : public GameObject
{
public:
    HPROPERTY(ScriptReadable | BlueprintReadWrite | Category="Item")
    std::string ItemName = "Unknown Item";
    
    HPROPERTY(ScriptReadable | BlueprintReadWrite | Category="Item")
    std::string Description = "No description available";
    
    HPROPERTY(BlueprintReadWrite | Category="Item")
    int Value = 0;
    
    HPROPERTY(Config | EditAnywhere | Category="Item")
    float Weight = 1.0f;
    
    HPROPERTY(SaveGame | BlueprintReadWrite | Category="State")
    bool IsEquipped = false;

public:
    Item() = default;
    
    HFUNCTION(BlueprintCallable | Category="Utility")
    void Use(Player* User)
    {
        if (User)
        {
            std::cout << "🎒 " << User->Name << " uses " << ItemName << std::endl;
            OnUsed(User);
        }
    }
    
    HFUNCTION(BlueprintEvent | Category="Events")
    void OnUsed(Player* User)
    {
        std::cout << "📦 " << ItemName << " was used by " << User->Name << std::endl;
    }
    
    HFUNCTION(BlueprintPure | Category="Info")
    std::string GetItemTooltip() const
    {
        return ItemName + "\n" + Description + "\nValue: " + std::to_string(Value) + " gold";
    }
};

// 反射系统演示器
class ReflectionSystemDemo
{
public:
    static void RunDemo()
    {
        std::cout << "🎮 Helianthus 高级反射系统演示" << std::endl;
        std::cout << "=================================" << std::endl;
        
        Demo1_BasicObjectCreation();
        Demo2_PropertyManipulation();
        Demo3_MethodInvocation();
        Demo4_InheritanceHierarchy();
        Demo5_GameSimulation();
        Demo6_ReflectionIntrospection();
        
        std::cout << "\n✅ 所有演示完成!" << std::endl;
    }

private:
    static void Demo1_BasicObjectCreation()
    {
        std::cout << "\n📋 演示1: 基础对象创建" << std::endl;
        std::cout << "------------------------" << std::endl;
        
        Player Hero;
        Hero.Name = "Aldric";
        Hero.PlayerClass = "Paladin";
        
        Enemy Goblin;
        Goblin.ConfigureEnemy("Goblin", 1);
        
        Item HealthPotion;
        HealthPotion.ItemName = "Health Potion";
        HealthPotion.Description = "Restores 50 health points";
        HealthPotion.Value = 25;
        
        std::cout << "创建对象:" << std::endl;
        std::cout << "  🧙 " << Hero.GetDebugInfo() << std::endl;
        std::cout << "  👹 " << Goblin.GetEnemyInfo() << std::endl;
        std::cout << "  🧪 " << HealthPotion.GetItemTooltip() << std::endl;
    }
    
    static void Demo2_PropertyManipulation()
    {
        std::cout << "\n🔧 演示2: 属性操作" << std::endl;
        std::cout << "-------------------" << std::endl;
        
        Player Mage;
        Mage.Name = "Elara";
        Mage.PlayerClass = "Mage";
        
        std::cout << "初始状态:" << std::endl;
        std::cout << "  " << Mage.GetDebugInfo() << std::endl;
        
        // 通过方法修改属性
        Mage.Move(10.0f, 5.0f, 0.0f);
        Mage.TakeDamage(30);
        Mage.Heal(20);
        
        std::cout << "修改后状态:" << std::endl;
        std::cout << "  " << Mage.GetDebugInfo() << std::endl;
    }
    
    static void Demo3_MethodInvocation()
    {
        std::cout << "\n⚡ 演示3: 方法调用" << std::endl;
        std::cout << "------------------" << std::endl;
        
        Player Warrior;
        Warrior.Name = "Grimlock";
        Warrior.PlayerClass = "Warrior";
        
        Enemy Orc;
        Orc.ConfigureEnemy("Orc", 2);
        
        std::cout << "战斗开始!" << std::endl;
        std::cout << "  战士: " << Warrior.GetDebugInfo() << std::endl;
        std::cout << "  兽人: " << Orc.GetEnemyInfo() << std::endl;
        
        // 回合制战斗
        while (Warrior.IsActive && Orc.IsActive)
        {
            Warrior.Attack(&Orc);
            if (Orc.IsActive)
            {
                Orc.PerformAttack(&Warrior);
            }
            std::cout << "  ---" << std::endl;
        }
        
        std::cout << "战斗结束!" << std::endl;
        if (Warrior.IsActive)
        {
            Warrior.AddExperience(Orc.ExperienceReward);
            Warrior.AddGold(Orc.GoldReward);
        }
    }
    
    static void Demo4_InheritanceHierarchy()
    {
        std::cout << "\n🏗️ 演示4: 继承层次结构" << std::endl;
        std::cout << "------------------------" << std::endl;
        
        // 展示继承关系
        std::vector<std::unique_ptr<Entity>> Entities;
        
        Entities.push_back(std::make_unique<Player>());
        Entities.push_back(std::make_unique<Enemy>());
        
        for (const auto& Entity : Entities)
        {
            std::cout << "实体类型: " << typeid(*Entity).name() << std::endl;
            std::cout << "  名称: " << Entity->Name << std::endl;
            std::cout << "  生命值: " << Entity->Health << "/" << Entity->MaxHealth << std::endl;
            std::cout << "  速度: " << Entity->MovementSpeed << std::endl;
            std::cout << "  ---" << std::endl;
        }
    }
    
    static void Demo5_GameSimulation()
    {
        std::cout << "\n🎲 演示5: 游戏模拟" << std::endl;
        std::cout << "-------------------" << std::endl;
        
        Player Hero;
        Hero.Name = "Aria";
        Hero.PlayerClass = "Rogue";
        
        std::vector<Enemy> Enemies;
        
        // 生成敌人
        for (int i = 1; i <= 3; ++i)
        {
            Enemy Monster;
            Monster.ConfigureEnemy("Skeleton", i);
            Enemies.push_back(Monster);
        }
        
        std::cout << "冒险开始!" << std::endl;
        std::cout << "英雄: " << Hero.GetDebugInfo() << std::endl;
        
        for (auto& Enemy : Enemies)
        {
            std::cout << "\n遭遇 " << Enemy.GetEnemyInfo() << std::endl;
            
            while (Hero.IsActive && Enemy.IsActive)
            {
                Hero.Attack(&Enemy);
                if (Enemy.IsActive)
                {
                    Enemy.PerformAttack(&Hero);
                }
            }
            
            if (!Hero.IsActive)
            {
                std::cout << "💀 英雄被击败了!" << std::endl;
                break;
            }
            
            std::cout << "🎉 胜利!" << std::endl;
            Hero.AddExperience(Enemy.ExperienceReward);
            Hero.AddGold(Enemy.GoldReward);
            
            // 使用治疗药水
            Item Potion;
            Potion.ItemName = "Minor Healing Potion";
            Potion.Use(&Hero);
            Hero.Heal(30);
        }
        
        std::cout << "\n冒险结束!" << std::endl;
        std::cout << "最终状态: " << Hero.GetDebugInfo() << std::endl;
    }
    
    static void Demo6_ReflectionIntrospection()
    {
        std::cout << "\n🔍 演示6: 反射内省" << std::endl;
        std::cout << "-------------------" << std::endl;
        
        // 模拟反射信息输出
        std::cout << "类信息:" << std::endl;
        std::cout << "  Player类:" << std::endl;
        std::cout << "    父类: Entity -> GameObject -> HObject" << std::endl;
        std::cout << "    标记: Scriptable, BlueprintType, ConfigClass, DefaultConfig" << std::endl;
        
        std::cout << "\n  属性列表:" << std::endl;
        std::cout << "    Level: int [ScriptReadable, BlueprintReadWrite, Category=Progress, SaveGame]" << std::endl;
        std::cout << "    Experience: int [ScriptReadable, BlueprintReadWrite, Category=Progress, SaveGame]" << std::endl;
        std::cout << "    PlayerClass: std::string [Config, EditAnywhere, Category=Player]" << std::endl;
        std::cout << "    Gold: int [SaveGame, BlueprintReadWrite, Category=Economy]" << std::endl;
        
        std::cout << "\n  方法列表:" << std::endl;
        std::cout << "    AddExperience(int): void [ScriptCallable, BlueprintCallable, Category=Progress]" << std::endl;
        std::cout << "    LevelUp(): void [ScriptCallable, BlueprintCallable, Category=Progress]" << std::endl;
        std::cout << "    Attack(Entity*): void [ScriptCallable, BlueprintCallable, Category=Combat]" << std::endl;
        std::cout << "    AddGold(int): void [BlueprintCallable, Category=Economy]" << std::endl;
    }
};

// 主函数
int main()
{
    std::cout << "🚀 Helianthus 反射系统高级演示启动" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try
    {
        ReflectionSystemDemo::RunDemo();
        
        std::cout << "\n🎯 反射系统演示完成!" << std::endl;
        std::cout << "📚 主要特性展示:" << std::endl;
        std::cout << "  ✅ HCLASS宏定义类" << std::endl;
        std::cout << "  ✅ HPROPERTY宏定义属性" << std::endl;
        std::cout << "  ✅ HFUNCTION宏定义方法" << std::endl;
        std::cout << "  ✅ 继承层次结构" << std::endl;
        std::cout << "  ✅ 元数据标记系统" << std::endl;
        std::cout << "  ✅ 运行时类型信息" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "❌ 错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}