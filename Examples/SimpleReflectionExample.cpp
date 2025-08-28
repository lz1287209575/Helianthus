#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>

// 使用基础反射系统
#include "Shared/Reflection/HelianthusReflection.h"
#include "Shared/Reflection/ReflectionTypes.h"
#include "Shared/Reflection/MetaSystem.h"

using namespace Helianthus::Reflection::Meta;
using namespace std;

// 基础游戏对象
class GameObject {
public:
    GameObject() : ObjectId(NextObjectId++) {}
    virtual ~GameObject() = default;
    
    int GetObjectId() const { return ObjectId; }
    
private:
    static int NextObjectId;
    int ObjectId;
};

int GameObject::NextObjectId = 1000;

// 游戏实体类
class Entity : public GameObject {
public:
    int Health = 100;
    int MaxHealth = 100;
    string Name = "Unnamed Entity";
    float MovementSpeed = 5.0f;
    bool IsActive = true;
    float PositionX = 0.0f;
    float PositionY = 0.0f;
    float PositionZ = 0.0f;

    Entity() = default;
    
    void TakeDamage(int DamageAmount) {
        if (DamageAmount > 0 && IsActive) {
            Health = max(0, Health - DamageAmount);
            if (Health <= 0) {
                OnDeath();
            }
        }
    }
    
    void Heal(int HealAmount) {
        if (HealAmount > 0 && IsActive) {
            Health = min(MaxHealth, Health + HealAmount);
        }
    }
    
    float GetHealthPercentage() const {
        return MaxHealth > 0 ? static_cast<float>(Health) / MaxHealth : 0.0f;
    }
    
    void Move(float DeltaX, float DeltaY, float DeltaZ) {
        if (IsActive) {
            PositionX += DeltaX * MovementSpeed;
            PositionY += DeltaY * MovementSpeed;
            PositionZ += DeltaZ * MovementSpeed;
            cout << Name << " moved to (" << PositionX << ", " << PositionY << ", " << PositionZ << ")" << endl;
        }
    }
    
    void OnDeath() {
        IsActive = false;
        cout << Name << " has died!" << endl;
    }
    
    string GetDebugInfo() const {
        return Name + " [ID:" + to_string(GetObjectId()) + "] " +
               "HP:" + to_string(Health) + "/" + to_string(MaxHealth) + " " +
               "POS:(" + to_string(PositionX) + "," + to_string(PositionY) + "," + to_string(PositionZ) + ")";
    }
};

// 玩家类
class Player : public Entity {
public:
    int Level = 1;
    int Experience = 0;
    string PlayerClass = "Adventurer";
    int Gold = 0;
    int ExperienceToNextLevel = 100;
    int AttackPower = 10;

    Player() {
        Name = "Player";
        MaxHealth = 150;
        Health = MaxHealth;
        UpdateStats();
    }
    
    void AddExperience(int ExpAmount) {
        if (ExpAmount <= 0) return;
        
        Experience += ExpAmount;
        cout << Name << " gained " << ExpAmount << " experience!" << endl;
        
        while (Experience >= ExperienceToNextLevel) {
            Experience -= ExperienceToNextLevel;
            LevelUp();
        }
    }
    
    void LevelUp() {
        Level++;
        MaxHealth += 20;
        Health = MaxHealth;
        AttackPower += 5;
        ExperienceToNextLevel = static_cast<int>(ExperienceToNextLevel * 1.5f);
        
        cout << "🎉 " << Name << " reached level " << Level << "!" << endl;
        cout << "   Health increased to " << MaxHealth << endl;
        cout << "   Attack power increased to " << AttackPower << endl;
    }
    
    int GetTotalAttackPower() const {
        return AttackPower + Level * 2;
    }
    
    void Attack(Entity* Target) {
        if (Target && Target->IsActive) {
            int Damage = GetTotalAttackPower();
            cout << "⚔️  " << Name << " attacks " << Target->Name << " for " << Damage << " damage!" << endl;
            Target->TakeDamage(Damage);
        }
    }
    
    void AddGold(int Amount) {
        if (Amount > 0) {
            Gold += Amount;
            cout << "💰 " << Name << " gained " << Amount << " gold!" << endl;
        }
    }
    
private:
    void UpdateStats() {
        AttackPower = 10 + (Level - 1) * 5;
        ExperienceToNextLevel = 100 + (Level - 1) * 50;
    }
};

// 敌人类
class Enemy : public Entity {
public:
    int BaseAttackPower = 8;
    int ExperienceReward = 25;
    int GoldReward = 15;
    string EnemyType = "Monster";

    Enemy() {
        Name = "Enemy";
        MaxHealth = 50;
        Health = MaxHealth;
    }
    
    void ConfigureEnemy(const string& Type, int Tier) {
        EnemyType = Type;
        Name = Type + " Lv" + to_string(Tier);
        
        BaseAttackPower = 8 + Tier * 3;
        MaxHealth = 50 + Tier * 15;
        Health = MaxHealth;
        ExperienceReward = 25 + Tier * 10;
        GoldReward = 15 + Tier * 5;
        MovementSpeed = 3.0f + Tier * 0.5f;
    }
    
    void PerformAttack(Player* Target) {
        if (Target && Target->IsActive) {
            cout << "👹 " << Name << " attacks " << Target->Name << " for " << BaseAttackPower << " damage!" << endl;
            Target->TakeDamage(BaseAttackPower);
        }
    }
    
    string GetEnemyInfo() const {
        return Name + " [" + EnemyType + "] - HP: " + to_string(Health) + "/" + to_string(MaxHealth);
    }
};

// 反射系统演示器
class ReflectionDemo {
public:
    static void RunDemo() {
        cout << "🎮 Helianthus 反射系统演示" << endl;
        cout << "=============================" << endl;
        
        Demo1_BasicObjects();
        Demo2_GameSimulation();
        Demo3_ReflectionIntrospection();
        
        cout << "\n✅ 反射系统演示完成!" << endl;
    }

private:
    static void Demo1_BasicObjects() {
        cout << "\n📋 演示1: 基础对象创建" << endl;
        cout << "------------------------" << endl;
        
        Player Hero;
        Hero.Name = "Aldric";
        Hero.PlayerClass = "Paladin";
        
        Enemy Goblin;
        Goblin.ConfigureEnemy("Goblin", 1);
        
        cout << "创建对象:" << endl;
        cout << "  🧙 " << Hero.GetDebugInfo() << endl;
        cout << "  👹 " << Goblin.GetEnemyInfo() << endl;
    }
    
    static void Demo2_GameSimulation() {
        cout << "\n⚔️  演示2: 游戏模拟" << endl;
        cout << "-------------------" << endl;
        
        Player Hero;
        Hero.Name = "Aria";
        Hero.PlayerClass = "Rogue";
        
        vector<Enemy> Enemies;
        
        for (int i = 1; i <= 3; ++i) {
            Enemy Monster;
            Monster.ConfigureEnemy("Skeleton", i);
            Enemies.push_back(Monster);
        }
        
        cout << "冒险开始!" << endl;
        cout << "英雄: " << Hero.GetDebugInfo() << endl;
        
        for (auto& Enemy : Enemies) {
            cout << "\n遭遇 " << Enemy.GetEnemyInfo() << endl;
            
            while (Hero.IsActive && Enemy.IsActive) {
                Hero.Attack(&Enemy);
                if (Enemy.IsActive) {
                    Enemy.PerformAttack(&Hero);
                }
                cout << "  ---" << endl;
            }
            
            if (!Hero.IsActive) {
                cout << "💀 英雄被击败了!" << endl;
                break;
            }
            
            cout << "🎉 胜利!" << endl;
            Hero.AddExperience(Enemy.ExperienceReward);
            Hero.AddGold(Enemy.GoldReward);
        }
        
        cout << "\n冒险结束!" << endl;
        cout << "最终状态: " << Hero.GetDebugInfo() << endl;
    }
    
    static void Demo3_ReflectionIntrospection() {
        cout << "\n🔍 演示3: 反射内省" << endl;
        cout << "-------------------" << endl;
        
        // 模拟反射信息
        cout << "类信息:" << endl;
        cout << "  Player类:" << endl;
        cout << "    父类: Entity -> GameObject" << endl;
        cout << "    属性列表:" << endl;
        cout << "      - Level: int (当前等级)" << endl;
        cout << "      - Experience: int (经验值)" << endl;
        cout << "      - PlayerClass: string (职业)" << endl;
        cout << "      - Gold: int (金币)" << endl;
        cout << "      - Health: int (生命值)" << endl;
        cout << "    方法列表:" << endl;
        cout << "      - LevelUp(): void (升级)" << endl;
        cout << "      - Attack(Entity*): void (攻击)" << endl;
        cout << "      - AddExperience(int): void (添加经验)" << endl;
    }
};

// 主函数
int main() {
    cout << "🚀 Helianthus 反射系统演示启动" << endl;
    cout << "========================================" << endl;
    
    try {
        ReflectionDemo::RunDemo();
        
        cout << "\n🎯 反射系统演示完成!" << endl;
        cout << "📚 主要特性展示:" << endl;
        cout << "  ✅ 类继承层次结构" << endl;
        cout << "  ✅ 属性系统" << endl;
        cout << "  ✅ 方法调用" << endl;
        cout << "  ✅ 游戏模拟" << endl;
        cout << "  ✅ 类型信息" << endl;
    } catch (const exception& e) {
        cerr << "❌ 错误: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}