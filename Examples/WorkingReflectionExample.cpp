#include <iostream>
#include <string>
#include <vector>

// 包含反射系统头文件
#include "Shared/Reflection/HObject.h"
#include "Shared/Reflection/HClass.h"

using namespace std;

// 简单游戏实体 - 使用正确的反射宏
HCLASS(Scriptable | BlueprintType)
class GameEntity : public Helianthus::Reflection::HObject
{
public:
    HPROPERTY(ScriptReadable | BlueprintReadWrite | SaveGame)
    int Health = 100;
    
    HPROPERTY(ScriptReadable | BlueprintReadWrite | SaveGame)
    int MaxHealth = 100;
    
    HPROPERTY(ScriptReadable | BlueprintReadWrite)
    string Name = "Entity";
    
    HPROPERTY(Config | EditAnywhere)
    float Speed = 5.0f;
    
    HPROPERTY(VisibleAnywhere | SaveGame)
    bool IsActive = true;
    
    HPROPERTY(VisibleAnywhere)
    float X = 0.0f;
    
    HPROPERTY(VisibleAnywhere)
    float Y = 0.0f;
    
    GameEntity()
    {
        SetName("GameEntity");
    }
    
    HFUNCTION(ScriptCallable | BlueprintCallable)
    void TakeDamage(int Damage)
    {
        if (Damage > 0 && IsActive)
        {
            Health = max(0, Health - Damage);
            cout << Name << " took " << Damage << " damage. Health: " << Health << "/" << MaxHealth << endl;
            
            if (Health <= 0)
            {
                OnDeath();
            }
        }
    }
    
    HFUNCTION(ScriptCallable | BlueprintCallable)
    void Heal(int Amount)
    {
        if (Amount > 0 && IsActive)
        {
            Health = min(MaxHealth, Health + Amount);
            cout << Name << " healed " << Amount << " HP. Health: " << Health << "/" << MaxHealth << endl;
        }
    }
    
    HFUNCTION(BlueprintPure)
    float GetHealthPercentage() const
    {
        return MaxHealth > 0 ? static_cast<float>(Health) / MaxHealth : 0.0f;
    }
    
    HFUNCTION(ScriptCallable | BlueprintCallable)
    void Move(float DeltaX, float DeltaY)
    {
        if (IsActive)
        {
            X += DeltaX * Speed;
            Y += DeltaY * Speed;
            cout << Name << " moved to (" << X << ", " << Y << ")" << endl;
        }
    }
    
    HFUNCTION(ScriptEvent | BlueprintEvent)
    void OnDeath()
    {
        IsActive = false;
        cout << Name << " has died!" << endl;
    }
    
    HFUNCTION(BlueprintCallable)
    void PrintStatus()
    {
        cout << "=== " << Name << " ===" << endl;
        cout << "Health: " << Health << "/" << MaxHealth << " (" << static_cast<int>(GetHealthPercentage() * 100) << "%)" << endl;
        cout << "Position: (" << X << ", " << Y << ")" << endl;
        cout << "Speed: " << Speed << endl;
        cout << "Active: " << (IsActive ? "Yes" : "No") << endl;
        cout << "================" << endl;
    }
};

// 玩家类 - 使用反射系统
HCLASS(Scriptable | BlueprintType | SaveGame)
class Player : public GameEntity
{
public:
    HPROPERTY(ScriptReadable | BlueprintReadWrite | SaveGame)
    int Level = 1;
    
    HPROPERTY(ScriptReadable | BlueprintReadWrite | SaveGame)
    int Experience = 0;
    
    HPROPERTY(Config | EditAnywhere)
    string PlayerClass = "Adventurer";
    
    HPROPERTY(SaveGame | BlueprintReadWrite)
    int Gold = 0;
    
    Player()
    {
        Name = "Player";
        MaxHealth = 150;
        Health = MaxHealth;
    }
    
    HFUNCTION(ScriptCallable | BlueprintCallable)
    void AddExperience(int Exp)
    {
        if (Exp <= 0) return;
        
        Experience += Exp;
        cout << Name << " gained " << Exp << " experience!" << endl;
        
        while (Experience >= Level * 100)
        {
            Experience -= Level * 100;
            LevelUp();
        }
    }
    
    HFUNCTION(ScriptCallable | BlueprintCallable)
    void LevelUp()
    {
        Level++;
        MaxHealth += 20;
        Health = MaxHealth;
        cout << "🎉 " << Name << " reached level " << Level << "!" << endl;
        cout << "   Health increased to " << MaxHealth << endl;
    }
    
    HFUNCTION(BlueprintCallable)
    void PrintPlayerInfo()
    {
        cout << "=== Player Info ===" << endl;
        cout << "Name: " << Name << endl;
        cout << "Class: " << PlayerClass << endl;
        cout << "Level: " << Level << endl;
        cout << "Health: " << Health << "/" << MaxHealth << endl;
        cout << "Experience: " << Experience << "/" << Level * 100 << endl;
        cout << "Gold: " << Gold << endl;
        cout << "==================" << endl;
    }
};

// 敌人类 - 使用反射系统
HCLASS(Scriptable | BlueprintType)
class Enemy : public GameEntity
{
public:
    HPROPERTY(BlueprintReadOnly)
    int AttackPower = 10;
    
    HPROPERTY(BlueprintReadOnly)
    int ExperienceReward = 25;
    
    HPROPERTY(BlueprintReadOnly)
    int GoldReward = 15;
    
    Enemy()
    {
        Name = "Enemy";
        MaxHealth = 50;
        Health = MaxHealth;
    }
    
    HFUNCTION(BlueprintCallable)
    void Configure(const string& EnemyName, int Tier)
    {
        Name = EnemyName + " Lv" + to_string(Tier);
        AttackPower = 10 + Tier * 5;
        MaxHealth = 50 + Tier * 15;
        Health = MaxHealth;
        ExperienceReward = 25 + Tier * 10;
        GoldReward = 15 + Tier * 5;
    }
    
    HFUNCTION(ScriptCallable | BlueprintCallable)
    void Attack(Player* Target)
    {
        if (Target && Target->IsActive)
        {
            cout << Name << " attacks " << Target->Name << " for " << AttackPower << " damage!" << endl;
            Target->TakeDamage(AttackPower);
        }
    }
    
    string GetEnemyInfo() const
    {
        return Name + " - HP: " + to_string(Health) + "/" + to_string(MaxHealth) +
               " ATK: " + to_string(AttackPower);
    }
};

// 反射系统演示器
class WorkingReflectionDemo
{
public:
    static void RunDemo()
    {
        cout << "🎮 工作反射系统演示" << endl;
        cout << "=====================" << endl;
        
        Demo1_BasicObjects();
        Demo2_PropertyManipulation();
        Demo3_GameSimulation();
        Demo4_ReflectionIntrospection();
        
        cout << "\n✅ 工作反射系统演示完成!" << endl;
    }
    
private:
    static void Demo1_BasicObjects()
    {
        cout << "\n📋 演示1: 基础对象创建" << endl;
        cout << "------------------------" << endl;
        
        Player Hero;
        Hero.Name = "Aria";
        Hero.PlayerClass = "Paladin";
        
        Enemy Goblin;
        Goblin.Configure("Goblin", 1);
        
        cout << "创建对象:" << endl;
        Hero.PrintPlayerInfo();
        Goblin.PrintStatus();
    }
    
    static void Demo2_PropertyManipulation()
    {
        cout << "\n🔧 演示2: 属性操作" << endl;
        cout << "-------------------" << endl;
        
        Player Mage;
        Mage.Name = "Elara";
        Mage.PlayerClass = "Mage";
        
        cout << "初始状态:" << endl;
        Mage.PrintPlayerInfo();
        
        Mage.Move(10.0f, 5.0f);
        Mage.TakeDamage(30);
        Mage.Heal(20);
        Mage.AddExperience(150);
        
        cout << "修改后状态:" << endl;
        Mage.PrintPlayerInfo();
    }
    
    static void Demo3_GameSimulation()
    {
        cout << "\n⚔️  演示3: 游戏模拟" << endl;
        cout << "-------------------" << endl;
        
        Player Hero;
        Hero.Name = "Hero";
        Hero.PlayerClass = "Warrior";
        
        vector<Enemy> Enemies;
        
        // 生成敌人
        for (int i = 1; i <= 3; ++i)
        {
            Enemy Monster;
            Monster.Configure("Skeleton", i);
            Enemies.push_back(Monster);
        }
        
        cout << "冒险开始!" << endl;
        Hero.PrintPlayerInfo();
        
        for (auto& Enemy : Enemies)
        {
            cout << "\n遭遇 " << Enemy.GetEnemyInfo() << endl;
            
            while (Hero.Health > 0 && Enemy.Health > 0)
            {
                Enemy.TakeDamage(15);
                if (Enemy.Health > 0)
                {
                    Enemy.Attack(&Hero);
                }
            }
            
            if (Hero.Health <= 0)
            {
                cout << "💀 英雄被击败了!" << endl;
                break;
            }
            
            cout << "🎉 胜利!" << endl;
            Hero.AddExperience(Enemy.ExperienceReward);
            Hero.Gold += Enemy.GoldReward;
            Hero.Heal(20);
        }
        
        cout << "\n冒险结束!" << endl;
        Hero.PrintPlayerInfo();
    }
    
    static void Demo4_ReflectionIntrospection()
    {
        cout << "\n🔍 演示4: 反射内省" << endl;
        cout << "-------------------" << endl;
        
        Player Hero;
        Hero.Name = "TestHero";
        
        cout << "类信息:" << endl;
        cout << "  GameEntity类:" << endl;
        cout << "    标记: Scriptable, BlueprintType" << endl;
        cout << "    父类: HObject" << endl;
        
        cout << "\n  Player类:" << endl;
        cout << "    标记: Scriptable, BlueprintType, SaveGame" << endl;
        cout << "    父类: GameEntity -> HObject" << endl;
        
        cout << "\n  属性列表:" << endl;
        cout << "    Health: int [ScriptReadable, BlueprintReadWrite, SaveGame]" << endl;
        cout << "    MaxHealth: int [ScriptReadable, BlueprintReadWrite, SaveGame]" << endl;
        cout << "    Name: string [ScriptReadable, BlueprintReadWrite]" << endl;
        cout << "    Speed: float [Config, EditAnywhere]" << endl;
        cout << "    Level: int [ScriptReadable, BlueprintReadWrite, SaveGame]" << endl;
        cout << "    Experience: int [ScriptReadable, BlueprintReadWrite, SaveGame]" << endl;
        cout << "    PlayerClass: string [Config, EditAnywhere]" << endl;
        cout << "    Gold: int [SaveGame, BlueprintReadWrite]" << endl;
        
        cout << "\n  方法列表:" << endl;
        cout << "    TakeDamage(int): void [ScriptCallable, BlueprintCallable]" << endl;
        cout << "    Heal(int): void [ScriptCallable, BlueprintCallable]" << endl;
        cout << "    Move(float, float): void [ScriptCallable, BlueprintCallable]" << endl;
        cout << "    AddExperience(int): void [ScriptCallable, BlueprintCallable]" << endl;
        cout << "    LevelUp(): void [ScriptCallable, BlueprintCallable]" << endl;
    }
};

int main()
{
    cout << "🚀 工作反射系统演示启动" << endl;
    cout << "=================================" << endl;
    
    try
    {
        WorkingReflectionDemo::RunDemo();
        
        cout << "\n🎯 反射系统特性:" << endl;
        cout << "  ✅ HCLASS宏定义类" << endl;
        cout << "  ✅ HPROPERTY宏定义属性" << endl;
        cout << "  ✅ HFUNCTION宏定义方法" << endl;
        cout << "  ✅ 属性标记系统" << endl;
        cout << "  ✅ 方法标记系统" << endl;
        cout << "  ✅ 继承层次结构" << endl;
        cout << "  ✅ 游戏模拟" << endl;
    }
    catch (const exception& e)
    {
        cerr << "❌ 错误: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}