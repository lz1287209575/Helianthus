#include <iostream>
#include <string>
#include <vector>

// 包含反射系统头文件
#include "Shared/Reflection/HObject.h"

using namespace std;

// 测试类 - 简化版本
class TestPlayer
{
public:
    // 属性
    int Health = 100;

    string Name = "TestPlayer";

    int Level = 1;

    float Experience = 0.0f;

    // 方法
    void TakeDamage(int Damage)
    {
        Health = max(0, Health - Damage);
        cout << Name << " took " << Damage << " damage, health now: " << Health << endl;
    }

    void AddExperience(float Exp)
    {
        Experience += Exp;
        cout << Name << " gained " << Exp << " experience" << endl;

        if (Experience >= Level * 100.0f)
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

    void PrintStatus()
    {
        cout << "=== Player Status ===" << endl;
        cout << "Name: " << Name << endl;
        cout << "Health: " << Health << endl;
        cout << "Level: " << Level << endl;
        cout << "Experience: " << Experience << endl;
        cout << "==================" << endl;
    }
};

// 测试反射系统
class ReflectionTest
{
public:
    static void RunTest()
    {
        cout << "🧪 Testing Reflection System" << endl;
        cout << "============================" << endl;

        TestPlayer Player;

        cout << "\n1. Initial state:" << endl;
        Player.PrintStatus();

        cout << "\n2. Testing methods:" << endl;
        Player.TakeDamage(30);
        Player.AddExperience(150.0f);

        cout << "\n3. Final state:" << endl;
        Player.PrintStatus();

        cout << "\n✅ Reflection test completed successfully!" << endl;
    }
};

int main()
{
    cout << "🚀 Helianthus Reflection System Test" << endl;
    cout << "====================================" << endl;

    try
    {
        ReflectionTest::RunTest();
        return 0;
    }
    catch (const exception& e)
    {
        cerr << "❌ Error: " << e.what() << endl;
        return 1;
    }
}