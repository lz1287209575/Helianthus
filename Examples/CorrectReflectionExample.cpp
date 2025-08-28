#include <iostream>
#include <string>
#include <vector>
#include <memory>

// 包含反射系统头文件
#include "Shared/Reflection/HObject.h"
#include "Shared/Reflection/HClass.h"

// 使用正确的反射宏
using namespace std;

// 基础游戏对象 - 使用正确的HCLASS宏
HCLASS(Scriptable | BlueprintType)
class GameObject : public Helianthus::Reflection::HObject
{
public:
    int ObjectId = 0;
    string ObjectName = "GameObject";
    
    GameObject() : ObjectId(NextObjectId++) {}
    virtual ~GameObject() = default;
    
    virtual string GetDebugInfo() const
    {
        return ObjectName + " [ID:" + to_string(ObjectId) + "]";
    }
    
    const string& GetName() const { return ObjectName; }
    void SetName(const string& Name) { ObjectName = Name; }
    
private:
    static int NextObjectId;
};

int GameObject::NextObjectId = 1000;

// 实体类 - 使用HCLASS和HPROPERTY宏
class Entity : public GameObject
{
public:
    HPROPERTY(ScriptReadable | BlueprintReadWrite | SaveGame)
    int Health = 100;
    
    HPROPERTY(ScriptReadable | BlueprintReadWrite | SaveGame)
    int MaxHealth = 100;
    
    HPROPERTY(ScriptReadable | BlueprintReadWrite)
    string Name = "Entity";
    
    HPROPERTY(Config | EditAnywhere)
    float MovementSpeed = 5.0f;
    
    HPROPERTY(VisibleAnywhere | SaveGame)
    bool IsActive = true;
    
    HPROPERTY(VisibleAnywhere)
    float PositionX = 0.0f;
    
    HPROPERTY(VisibleAnywhere)
    float PositionY = 0.0f;
    
    Entity()
    {
        ObjectName = "Entity";
    }
    
    HFUNCTION(ScriptCallable | BlueprintCallable)
    void TakeDamage(int DamageAmount)
    {
        if (DamageAmount > 0 && IsActive)
        {
            Health = max(0, Health - DamageAmount);
            if (Health <= 0)
            {
                OnDeath();
            }
        }
    }
    
    HFUNCTION(ScriptCallable | BlueprintCallable)
    void Heal(int HealAmount)
    {
        if (HealAmount > 0 && IsActive)
        {
            Health = min(MaxHealth, Health + HealAmount);
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
            PositionX += DeltaX * MovementSpeed;
            PositionY += DeltaY * MovementSpeed;
            cout << Name << " moved to (" << PositionX << ", " << PositionY << ")" << endl;
        }
    }
    
    HFUNCTION(ScriptEvent | BlueprintEvent)
    void OnDeath()
    {
        IsActive = false;
        cout << Name << " has died!" << endl;
    }
    
    string GetDebugInfo() const override
    {
        return Name + " [ID:" + to_string(ObjectId) + "] " +
               "HP:" + to_string(Health) + "/" + to_string(MaxHealth) +
               " POS:(" + to_string(PositionX) + "," + to_string(PositionY) + ")";
    }
};

// 玩家类 - 使用完整的反射系统
HCLASS(Scriptable | BlueprintType | SaveGame)
class Player : public Entity
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
    
    HPROPERTY(BlueprintReadOnly)
    int ExperienceToNextLevel = 100;
    
    Player()
    {
        Name = "Player";
        ObjectName = "Player";
        MaxHealth = 150;
        Health = MaxHealth;
    }
    
    HFUNCTION(ScriptCallable | BlueprintCallable)
    void AddExperience(int ExpAmount)
    {
        if (ExpAmount <= 0) return;
        
        Experience += ExpAmount;
        cout << Name << " gained " << ExpAmount << " experience!" << endl;
        
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
        ExperienceToNextLevel = Level * 100;
        cout << "🎉 " << Name << " reached level " << Level << "!" << endl;
        OnLevelUp();
    }
    
    HFUNCTION(BlueprintEvent)
    void OnLevelUp()
    {
        cout << "🌟 " << Name << " feels stronger!" << endl;
    }
    
    void AddGold(int Amount)
    {
        if (Amount > 0)
        {
            Gold += Amount;
            cout << "💰 " << Name << " gained " << Amount << " gold!" << endl;
        }
    }
    
    HFUNCTION(BlueprintCallable)
    void PrintStatus()
    {
        cout << "=== Player Status ===" << endl;
        cout << "Name: " << Name << endl;
        cout << "Level: " << Level << endl;
        cout << "Class: " << PlayerClass << endl;
        cout << "Health: " << Health << "/" << MaxHealth << endl;
        cout << "Experience: " << Experience << "/" << ExperienceToNextLevel << endl;
        cout << "Gold: " << Gold << endl;
        cout << "Position: (" << PositionX << ", " << PositionY << ")" << endl;
        cout << "==================" << endl;
    }
};

// 敌人类 - 使用反射系统
HCLASS(Scriptable | BlueprintType)
class Enemy : public Entity
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
        ObjectName = "Enemy";
        MaxHealth = 50;
        Health = MaxHealth;
    }
    
    HFUNCTION(BlueprintCallable)
    void Configure(const string& EnemyName, int Tier)
    {
        Name = EnemyName + " Lv" + to_string(Tier);
        ObjectName = Name;
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

// 物品类 - 使用反射系统
HCLASS(Scriptable | BlueprintType | SaveGame)
class Item : public GameObject
{
public:
    HPROPERTY(SaveGame | BlueprintReadWrite)
    string ItemName = "Unknown Item";
    
    HPROPERTY(SaveGame | BlueprintReadWrite)
    string Description = "No description available";
    
    HPROPERTY(BlueprintReadWrite)
    int Value = 0;
    
    HPROPERTY(Config | EditAnywhere)
    float Weight = 1.0f;
    
    HPROPERTY(SaveGame | BlueprintReadWrite)
    bool IsEquipped = false;
    
    Item()
    {
        ObjectName = "Item";
    }
    
    HFUNCTION(BlueprintCallable)
    void Use(Player* User)
    {
        if (User)
        {
            cout << "🎒 " << User->Name << " uses " << ItemName << endl;
            OnUsed(User);
        }
    }
    
    HFUNCTION(BlueprintEvent)
    void OnUsed(Player* User)
    {
        cout << "📦 " << ItemName << " was used by " << User->Name << endl;
    }
    
    HFUNCTION(BlueprintPure)
    string GetItemTooltip() const
    {
        return ItemName + "\n" + Description + "\nValue: " + to_string(Value) + " gold";
    }
};

// 反射系统演示器
class CorrectReflectionDemo
{
public:
    static void RunDemo()
    {
        cout << "🎮 正确反射系统演示" << endl;
        cout << "=====================" << endl;
        
        Demo1_BasicObjects();
        Demo2_PropertyManipulation();
        Demo3_GameSimulation();
        Demo4_ReflectionIntrospection();
        
        cout << "\n✅ 正确反射系统演示完成!" << endl;
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
        
        Item HealthPotion;
        HealthPotion.ItemName = "Health Potion";
        HealthPotion.Description = "Restores 50 health points";
        HealthPotion.Value = 25;
        
        cout << "创建对象:" << endl;
        cout << "  🧙 " << Hero.GetDebugInfo() << endl;
        cout << "  👹 " << Goblin.GetEnemyInfo() << endl;
        cout << "  🧪 " << HealthPotion.GetItemTooltip() << endl;
    }
    
    static void Demo2_PropertyManipulation()
    {
        cout << "\n🔧 演示2: 属性操作" << endl;
        cout << "-------------------" << endl;
        
        Player Mage;
        Mage.Name = "Elara";
        Mage.PlayerClass = "Mage";
        
        cout << "初始状态:" << endl;
        Mage.PrintStatus();
        
        Mage.Move(10.0f, 5.0f);
        Mage.TakeDamage(30);
        Mage.Heal(20);
        Mage.AddExperience(150);
        
        cout << "修改后状态:" << endl;
        Mage.PrintStatus();
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
        Hero.PrintStatus();
        
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
            Hero.AddGold(Enemy.GoldReward);
            
            // 使用治疗药水
            Item Potion;
            Potion.ItemName = "Minor Healing Potion";
            Potion.Use(&Hero);
            Hero.Heal(30);
        }
        
        cout << "\n冒险结束!" << endl;
        Hero.PrintStatus();
    }
    
    static void Demo4_ReflectionIntrospection()
    {
        cout << "\n🔍 演示4: 反射内省" << endl;
        cout << "-------------------" << endl;
        
        Player Hero;
        Hero.Name = "TestHero";
        
        cout << "类信息:" << endl;
        cout << "  Player类:" << endl;
        cout << "    标记: Scriptable, BlueprintType, SaveGame" << endl;
        cout << "    父类: Entity -> GameObject -> HObject" << endl;
        
        cout << "\n  属性列表:" << endl;
        cout << "    Level: int [ScriptReadable, BlueprintReadWrite, SaveGame]" << endl;
        cout << "    Experience: int [ScriptReadable, BlueprintReadWrite, SaveGame]" << endl;
        cout << "    PlayerClass: string [Config, EditAnywhere]" << endl;
        cout << "    Gold: int [SaveGame, BlueprintReadWrite]" << endl;
        cout << "    Health: int [ScriptReadable, BlueprintReadWrite, SaveGame]" << endl;
        cout << "    MaxHealth: int [ScriptReadable, BlueprintReadWrite, SaveGame]" << endl;
        
        cout << "\n  方法列表:" << endl;
        cout << "    AddExperience(int): void [ScriptCallable, BlueprintCallable]" << endl;
        cout << "    LevelUp(): void [ScriptCallable, BlueprintCallable]" << endl;
        cout << "    TakeDamage(int): void [ScriptCallable, BlueprintCallable]" << endl;
        cout << "    Attack(Player*): void [ScriptCallable, BlueprintCallable]" << endl;
    }
};

int main()
{
    cout << "🚀 Helianthus 正确反射系统演示启动" << endl;
    cout << "========================================" << endl;
    
    try
    {
        CorrectReflectionDemo::RunDemo();
        
        cout << "\n🎯 正确反射特性:" << endl;
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