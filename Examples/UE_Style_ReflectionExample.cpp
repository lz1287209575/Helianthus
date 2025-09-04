#include "Shared/Common/Logger.h"
#include "Shared/Reflection/UE_Style_Macros.h"
#include "Shared/Reflection/UE_Style_Reflection.h"

#include <iostream>
#include <memory>
#include <string>

using namespace Helianthus::Reflection;
using namespace Helianthus::Common;

// UE 风格的基础对象类
class UObject
{
public:
    virtual ~UObject() = default;

    UPROPERTY(EditAnywhere, "Basic", std::string, Name)
    std::string Name;

    UPROPERTY(EditAnywhere, "Basic", int32_t, ID)
    int32_t ID;

    UObject() : Name("Unknown"), ID(0) {}
    UObject(const std::string& name, int32_t id) : Name(name), ID(id) {}

    UFUNCTION(BlueprintCallable, "Basic", void, SetName, const std::string&)
    void SetName(const std::string& name)
    {
        Name = name;
    }

    UFUNCTION(BlueprintCallable, "Basic", std::string, GetName)
    std::string GetName() const
    {
        return Name;
    }

    GENERATED_BODY()
};

// UE 风格的玩家类
class UPlayer : public UObject
{
public:
    UPROPERTY(EditAnywhere, "Player", int32_t, Health)
    int32_t Health;

    UPROPERTY(EditAnywhere, "Player", float, Speed)
    float Speed;

    UPROPERTY(BlueprintReadOnly, "Player", bool, IsAlive)
    bool IsAlive;

    UPlayer() : Health(100), Speed(1.0f), IsAlive(true) {}
    UPlayer(const std::string& name, int32_t health, float speed)
        : UObject(name, 0), Health(health), Speed(speed), IsAlive(true)
    {
    }

    UFUNCTION(BlueprintCallable, "Player", void, TakeDamage, int32_t)
    void TakeDamage(int32_t damage)
    {
        Health -= damage;
        if (Health <= 0)
        {
            Health = 0;
            IsAlive = false;
        }
    }

    UFUNCTION(BlueprintCallable, "Player", void, Heal, int32_t)
    void Heal(int32_t amount)
    {
        Health += amount;
        if (Health > 0)
            IsAlive = true;
    }

    UFUNCTION(BlueprintPure, "Player", bool, IsPlayerAlive)
    bool IsPlayerAlive() const
    {
        return IsAlive;
    }

    UFUNCTION(BlueprintCallable, "Player", std::string, GetStatus)
    std::string GetStatus() const
    {
        return Name + " (HP: " + std::to_string(Health) + ", Speed: " + std::to_string(Speed) + ")";
    }

    GENERATED_BODY()
};

// UE 风格的武器类
class UWeapon : public UObject
{
public:
    UPROPERTY(EditAnywhere, "Weapon", int32_t, Damage)
    int32_t Damage;

    UPROPERTY(EditAnywhere, "Weapon", float, Range)
    float Range;

    UPROPERTY(EditAnywhere, "Weapon", std::string, WeaponType)
    std::string WeaponType;

    UWeapon() : Damage(10), Range(1.5f), WeaponType("Sword") {}
    UWeapon(const std::string& name, int32_t damage, float range, const std::string& type)
        : UObject(name, 0), Damage(damage), Range(range), WeaponType(type)
    {
    }

    UFUNCTION(BlueprintCallable, "Weapon", void, Upgrade)
    void Upgrade()
    {
        Damage += 5;
    }

    UFUNCTION(BlueprintPure, "Weapon", bool, IsRanged)
    bool IsRanged() const
    {
        return Range > 2.0f;
    }

    UFUNCTION(BlueprintCallable, "Weapon", std::string, GetDescription)
    std::string GetDescription() const
    {
        return Name + " (" + WeaponType + ", DMG: " + std::to_string(Damage) + ")";
    }

    GENERATED_BODY()
};

// UE 风格的枚举
UENUM(BlueprintType, EWeaponType)
enum class EWeaponType : int32_t
{
    Sword = 0,
    Axe = 1,
    Bow = 2,
    Staff = 3,
    Dagger = 4
};

int main()
{
    try
    {
        Logger::Info("=== UE 风格反射系统示例 ===");

        // 1. 初始化 UE 风格反射系统
        Logger::Info("1. 初始化 UE 风格反射系统");
        InitializeUReflectionSystem();

        // 2. 注册 UE 风格类
        Logger::Info("2. 注册 UE 风格类");

        // 自动注册类
        HELIANTHUS_AUTO_REGISTER_UCLASS(UObject);
        HELIANTHUS_AUTO_REGISTER_UCLASS(UPlayer);
        HELIANTHUS_AUTO_REGISTER_UCLASS(UWeapon);

        // 3. 查询 UE 风格类型信息
        Logger::Info("3. 查询 UE 风格类型信息");

        auto& UReflectionSystem = UReflectionSystem::GetInstance();
        auto UClassNames = UReflectionSystem.GetAllUClassNames();

        Logger::Info("已注册的 UE 风格类 (" + std::to_string(UClassNames.size()) + "):");
        for (const auto& Name : UClassNames)
        {
            Logger::Info("  - " + Name);

            const UClassInfo* Info = UReflectionSystem.GetUClassInfo(Name);
            if (Info)
            {
                auto PropertyNames = UReflectionSystem.GetAllUPropertyNames(Name);
                Logger::Info("    属性 (" + std::to_string(PropertyNames.size()) + "):");
                for (const auto& PropName : PropertyNames)
                {
                    const UPropertyInfo* PropInfo =
                        UReflectionSystem.GetUPropertyInfo(Name, PropName);
                    if (PropInfo)
                    {
                        Logger::Info("      - " + PropName + " (" + PropInfo->TypeName + ") [" +
                                     PropInfo->Category + "]");
                    }
                }

                auto FunctionNames = UReflectionSystem.GetAllUFunctionNames(Name);
                Logger::Info("    函数 (" + std::to_string(FunctionNames.size()) + "):");
                for (const auto& FuncName : FunctionNames)
                {
                    const UFunctionInfo* FuncInfo =
                        UReflectionSystem.GetUFunctionInfo(Name, FuncName);
                    if (FuncInfo)
                    {
                        Logger::Info("      - " + FuncName + " -> " + FuncInfo->ReturnTypeName +
                                     " [" + FuncInfo->Category + "]");
                    }
                }
            }
        }

        // 4. 创建和使用 UE 风格对象
        Logger::Info("4. 创建和使用 UE 风格对象");

        // 创建对象
        void* PlayerObj = UReflectionSystem.CreateUObject("UPlayer");
        void* WeaponObj = UReflectionSystem.CreateUObject("UWeapon");

        if (PlayerObj && WeaponObj)
        {
            Logger::Info("成功创建 UE 风格对象");

            // 设置属性
            std::string PlayerName = "Hero";
            UReflectionSystem.SetUProperty(PlayerObj, "Name", &PlayerName);

            int32_t PlayerHealth = 150;
            UReflectionSystem.SetUProperty(PlayerObj, "Health", &PlayerHealth);

            float PlayerSpeed = 1.2f;
            UReflectionSystem.SetUProperty(PlayerObj, "Speed", &PlayerSpeed);

            std::string WeaponName = "MagicSword";
            UReflectionSystem.SetUProperty(WeaponObj, "Name", &WeaponName);

            int32_t WeaponDamage = 25;
            UReflectionSystem.SetUProperty(WeaponObj, "Damage", &WeaponDamage);

            // 调用函数
            std::vector<void*> Args;
            int32_t Damage = 30;
            Args.push_back(&Damage);
            UReflectionSystem.CallUFunction(PlayerObj, "TakeDamage", Args);

            // 获取属性
            void* HealthValue = UReflectionSystem.GetUProperty(PlayerObj, "Health");
            if (HealthValue)
            {
                int32_t CurrentHealth = *static_cast<int32_t*>(HealthValue);
                Logger::Info("Player 当前血量: " + std::to_string(CurrentHealth));
            }

            // 调用纯函数
            Args.clear();
            void* StatusResult = UReflectionSystem.CallUFunction(PlayerObj, "GetStatus", Args);
            if (StatusResult)
            {
                std::string Status = *static_cast<std::string*>(StatusResult);
                Logger::Info("Player 状态: " + Status);
            }

            // 销毁对象
            UReflectionSystem.DestroyUObject("UPlayer", PlayerObj);
            UReflectionSystem.DestroyUObject("UWeapon", WeaponObj);
        }

        // 5. 生成 UE 风格代码
        Logger::Info("5. 生成 UE 风格代码");

        std::string UClassCode = UReflectionSystem.GenerateUClassCode("UPlayer");
        Logger::Info("生成的 UPlayer 类代码长度: " + std::to_string(UClassCode.length()));

        if (!UClassCode.empty())
        {
            Logger::Info("UPlayer 类代码预览:");
            std::cout << UClassCode.substr(0, 500) << "..." << std::endl;
        }

        // 6. 生成脚本绑定
        Logger::Info("6. 生成脚本绑定");

        std::string ScriptBindings = UReflectionSystem.GenerateScriptBindings("lua");
        Logger::Info("生成的脚本绑定代码长度: " + std::to_string(ScriptBindings.length()));

        if (!ScriptBindings.empty())
        {
            Logger::Info("脚本绑定代码预览:");
            std::cout << ScriptBindings.substr(0, 500) << "..." << std::endl;
        }

        // 保存脚本绑定
        bool SaveResult = UReflectionSystem.SaveScriptBindings("ue_style_bindings.lua", "lua");
        if (SaveResult)
        {
            Logger::Info("UE 风格脚本绑定已保存到 ue_style_bindings.lua");
        }
        else
        {
            Logger::Warn("保存 UE 风格脚本绑定失败");
        }

        // 7. 演示实际对象操作
        Logger::Info("7. 演示实际对象操作");

        // 创建实际对象
        UPlayer Player("Hero", 150, 1.2f);
        UWeapon Weapon("MagicSword", 25, 2.5f, "Sword");

        Logger::Info("Player 状态: " + Player.GetStatus());
        Logger::Info("Weapon 描述: " + Weapon.GetDescription());

        // 模拟游戏操作
        Player.TakeDamage(30);
        Logger::Info("Player 受到 30 点伤害后: " + Player.GetStatus());

        Player.Heal(20);
        Logger::Info("Player 恢复 20 点血量后: " + Player.GetStatus());

        Weapon.Upgrade();
        Logger::Info("Weapon 升级后: " + Weapon.GetDescription());

        // 8. 清理
        Logger::Info("8. 清理资源");
        ShutdownUReflectionSystem();

        Logger::Info("=== UE 风格反射系统示例完成 ===");
    }
    catch (const std::exception& e)
    {
        Logger::Error("UE 风格示例运行出错: " + std::string(e.what()));
        return 1;
    }

    return 0;
}
