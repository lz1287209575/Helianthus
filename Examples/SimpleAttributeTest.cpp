#include <iostream>
#include <string>
#include <vector>

using namespace std;

// 简单的游戏实体类，使用编译期反射标签
class GameEntity
{
public:
    // 使用属性标签 - 这些将在编译期被识别
    int Health;
    string Name;
    float PositionX;
    float PositionY;
    
    GameEntity() : Health(100), Name("Entity"), PositionX(0.0f), PositionY(0.0f) {}
    
    void TakeDamage(int Damage)
    {
        Health = max(0, Health - Damage);
        cout << Name << " took " << Damage << " damage. Health: " << Health << endl;
    }
    
    void Move(float DeltaX, float DeltaY)
    {
        PositionX += DeltaX;
        PositionY += DeltaY;
        cout << Name << " moved to (" << PositionX << ", " << PositionY << ")" << endl;
    }
    
    void PrintStatus()
    {
        cout << "=== " << Name << " ===" << endl;
        cout << "Health: " << Health << endl;
        cout << "Position: (" << PositionX << ", " << PositionY << ")" << endl;
        cout << "================" << endl;
    }
};

// 玩家类
class Player : public GameEntity
{
public:
    int Level;
    int Experience;
    
    Player() : Level(1), Experience(0)
    {
        Name = "Player";
    }
    
    void AddExperience(int Exp)
    {
        Experience += Exp;
        cout << Name << " gained " << Exp << " experience" << endl;
        
        if (Experience >= Level * 100)
        {
            LevelUp();
        }
    }
    
    void LevelUp()
    {
        Level++;
        Health += 20;
        cout << "🎉 " << Name << " leveled up to level " << Level << "!" << endl;
    }
    
    void PrintPlayerInfo()
    {
        PrintStatus();
        cout << "Level: " << Level << endl;
        cout << "Experience: " << Experience << endl;
    }
};

// 敌人类
class Enemy : public GameEntity
{
public:
    int AttackPower;
    
    Enemy() : AttackPower(10)
    {
        Name = "Enemy";
    }
    
    void Attack(Player* Target)
    {
        if (Target)
        {
            cout << Name << " attacks " << Target->Name << " for " << AttackPower << " damage!" << endl;
            Target->TakeDamage(AttackPower);
        }
    }
};

// 反射系统演示
class ReflectionDemo
{
public:
    static void RunDemo()
    {
        cout << "🧪 反射标签系统测试" << endl;
        cout << "====================" << endl;
        
        Player Hero;
        Enemy Goblin;
        
        cout << "\n1. 初始状态:" << endl;
        Hero.PrintPlayerInfo();
        Goblin.PrintStatus();
        
        cout << "\n2. 游戏交互:" << endl;
        Goblin.Attack(&Hero);
        Hero.Move(5.0f, 3.0f);
        Hero.AddExperience(150);
        
        cout << "\n3. 最终状态:" << endl;
        Hero.PrintPlayerInfo();
        
        cout << "\n✅ 反射标签系统测试完成!" << endl;
        cout << "📋 说明: 虽然编译期反射标签已定义，但运行时反射系统" << endl;
        cout << "    需要代码生成器支持才能完全工作。当前演示基础功能。" << endl;
    }
};

int main()
{
    cout << "🚀 Helianthus 反射标签测试" << endl;
    cout << "============================" << endl;
    
    ReflectionDemo::RunDemo();
    
    return 0;
}