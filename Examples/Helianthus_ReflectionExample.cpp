#include "Shared/Reflection/Helianthus_Reflection.h"
#include "Shared/Reflection/Helianthus_Macros.h"
#include "Shared/Common/Logger.h"

#include <iostream>
#include <memory>
#include <string>

using namespace Helianthus::Reflection;
using namespace Helianthus::Common;

// Helianthus 风格的基础对象类
class HObject
{
public:
    virtual ~HObject() = default;
    
    // 简化的属性声明
    std::string Name;
    int32_t ID;
    
    HObject() : Name("Unknown"), ID(0) {}
    HObject(const std::string& name, int32_t id) : Name(name), ID(id) {}
    
    // 简化的函数声明
    void SetName(const std::string& name) { Name = name; }
    std::string GetName() const { return Name; }
};

// Helianthus 风格的玩家类
class HPlayer : public HObject
{
public:
    // 简化的属性声明
    int32_t Health;
    float Speed;
    bool IsAlive;
    
    HPlayer() : Health(100), Speed(1.0f), IsAlive(true) {}
    HPlayer(const std::string& name, int32_t health, float speed) 
        : HObject(name, 0), Health(health), Speed(speed), IsAlive(true) {}
    
    // 简化的函数声明
    void TakeDamage(int32_t damage) {
        Health -= damage;
        if (Health <= 0) {
            Health = 0;
            IsAlive = false;
        }
    }
    
    void Heal(int32_t amount) {
        Health += amount;
        if (Health > 0) IsAlive = true;
    }
    
    bool IsPlayerAlive() const { return IsAlive; }
    
    std::string GetStatus() const {
        return Name + " (HP: " + std::to_string(Health) + ", Speed: " + std::to_string(Speed) + ")";
    }
};

// Helianthus 风格的武器类
class HWeapon : public HObject
{
public:
    // 简化的属性声明
    int32_t Damage;
    float Range;
    std::string WeaponType;
    
    HWeapon() : Damage(10), Range(1.5f), WeaponType("Sword") {}
    HWeapon(const std::string& name, int32_t damage, float range, const std::string& type)
        : HObject(name, 0), Damage(damage), Range(range), WeaponType(type) {}
    
    // 简化的函数声明
    void Upgrade() { Damage += 5; }
    bool IsRanged() const { return Range > 2.0f; }
    
    std::string GetDescription() const {
        return Name + " (" + WeaponType + ", DMG: " + std::to_string(Damage) + ")";
    }
};

// Helianthus 风格的枚举
enum class HWeaponType : int32_t
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
        Logger::Info("=== Helianthus 风格反射系统示例 ===");
        
        // 1. 初始化 Helianthus 风格反射系统
        Logger::Info("1. 初始化 Helianthus 风格反射系统");
        InitializeHelianthusReflectionSystem();
        
        // 2. 手动注册 Helianthus 风格类（模拟宏的效果）
        Logger::Info("2. 注册 Helianthus 风格类");
        
        auto& HelianthusReflectionSystem = HelianthusReflectionSystem::GetInstance();
        
        // 注册 HObject 类
        HClassInfo HObjectClass;
        HObjectClass.ClassName = "HObject";
        HObjectClass.BaseClassName = "";
        HObjectClass.TypeIndex = std::type_index(typeid(HObject));
        HObjectClass.Flags = HClassFlags::Scriptable;
        HObjectClass.Categories.push_back("Basic");
        HObjectClass.Constructor = [](void*) -> void* { return new HObject(); };
        HObjectClass.Destructor = [](void* Obj) { delete static_cast<HObject*>(Obj); };
        
        // 添加 HObject 属性
        HPropertyInfo NameProp;
        NameProp.PropertyName = "Name";
        NameProp.TypeName = "std::string";
        NameProp.Type = ReflectionType::String;
        NameProp.Flags = HPropertyFlags::EditAnywhere;
        NameProp.Category = "Basic";
        NameProp.Getter = [](void* Obj) -> void* { return &static_cast<HObject*>(Obj)->Name; };
        NameProp.Setter = [](void* Obj, void* Value) { static_cast<HObject*>(Obj)->Name = *static_cast<std::string*>(Value); };
        HObjectClass.Properties.push_back(NameProp);
        
        HPropertyInfo IDProp;
        IDProp.PropertyName = "ID";
        IDProp.TypeName = "int32_t";
        IDProp.Type = ReflectionType::Int32;
        IDProp.Flags = HPropertyFlags::EditAnywhere;
        IDProp.Category = "Basic";
        IDProp.Getter = [](void* Obj) -> void* { return &static_cast<HObject*>(Obj)->ID; };
        IDProp.Setter = [](void* Obj, void* Value) { static_cast<HObject*>(Obj)->ID = *static_cast<int32_t*>(Value); };
        HObjectClass.Properties.push_back(IDProp);
        
        HelianthusReflectionSystem.RegisterHClass(HObjectClass);
        
        // 注册 HPlayer 类
        HClassInfo HPlayerClass;
        HPlayerClass.ClassName = "HPlayer";
        HPlayerClass.BaseClassName = "HObject";
        HPlayerClass.TypeIndex = std::type_index(typeid(HPlayer));
        HPlayerClass.Flags = HClassFlags::Scriptable | HClassFlags::BlueprintType;
        HPlayerClass.Categories.push_back("Player");
        HPlayerClass.Constructor = [](void*) -> void* { return new HPlayer(); };
        HPlayerClass.Destructor = [](void* Obj) { delete static_cast<HPlayer*>(Obj); };
        
        // 添加 HPlayer 属性
        HPropertyInfo HealthProp;
        HealthProp.PropertyName = "Health";
        HealthProp.TypeName = "int32_t";
        HealthProp.Type = ReflectionType::Int32;
        HealthProp.Flags = HPropertyFlags::EditAnywhere | HPropertyFlags::BlueprintReadWrite;
        HealthProp.Category = "Player";
        HealthProp.Getter = [](void* Obj) -> void* { return &static_cast<HPlayer*>(Obj)->Health; };
        HealthProp.Setter = [](void* Obj, void* Value) { static_cast<HPlayer*>(Obj)->Health = *static_cast<int32_t*>(Value); };
        HPlayerClass.Properties.push_back(HealthProp);
        
        HPropertyInfo SpeedProp;
        SpeedProp.PropertyName = "Speed";
        SpeedProp.TypeName = "float";
        SpeedProp.Type = ReflectionType::Float;
        SpeedProp.Flags = HPropertyFlags::EditAnywhere | HPropertyFlags::BlueprintReadWrite;
        SpeedProp.Category = "Player";
        SpeedProp.Getter = [](void* Obj) -> void* { return &static_cast<HPlayer*>(Obj)->Speed; };
        SpeedProp.Setter = [](void* Obj, void* Value) { static_cast<HPlayer*>(Obj)->Speed = *static_cast<float*>(Value); };
        HPlayerClass.Properties.push_back(SpeedProp);
        
        HPropertyInfo IsAliveProp;
        IsAliveProp.PropertyName = "IsAlive";
        IsAliveProp.TypeName = "bool";
        IsAliveProp.Type = ReflectionType::Bool;
        IsAliveProp.Flags = HPropertyFlags::BlueprintReadOnly;
        IsAliveProp.Category = "Player";
        IsAliveProp.Getter = [](void* Obj) -> void* { return &static_cast<HPlayer*>(Obj)->IsAlive; };
        IsAliveProp.Setter = [](void* Obj, void* Value) { static_cast<HPlayer*>(Obj)->IsAlive = *static_cast<bool*>(Value); };
        HPlayerClass.Properties.push_back(IsAliveProp);
        
        // 添加 HPlayer 函数
        HFunctionInfo TakeDamageFunc;
        TakeDamageFunc.FunctionName = "TakeDamage";
        TakeDamageFunc.ReturnTypeName = "void";
        TakeDamageFunc.ReturnType = ReflectionType::Void;
        TakeDamageFunc.Flags = HFunctionFlags::BlueprintCallable;
        TakeDamageFunc.Category = "Player";
        TakeDamageFunc.Invoker = [](void* Obj, const std::vector<void*>& Args) -> void* {
            if (Args.size() >= 1) {
                int32_t damage = *static_cast<int32_t*>(Args[0]);
                static_cast<HPlayer*>(Obj)->TakeDamage(damage);
            }
            return nullptr;
        };
        HPlayerClass.Methods.push_back(TakeDamageFunc);
        
        HFunctionInfo HealFunc;
        HealFunc.FunctionName = "Heal";
        HealFunc.ReturnTypeName = "void";
        HealFunc.ReturnType = ReflectionType::Void;
        HealFunc.Flags = HFunctionFlags::BlueprintCallable;
        HealFunc.Category = "Player";
        HealFunc.Invoker = [](void* Obj, const std::vector<void*>& Args) -> void* {
            if (Args.size() >= 1) {
                int32_t amount = *static_cast<int32_t*>(Args[0]);
                static_cast<HPlayer*>(Obj)->Heal(amount);
            }
            return nullptr;
        };
        HPlayerClass.Methods.push_back(HealFunc);
        
        HFunctionInfo GetStatusFunc;
        GetStatusFunc.FunctionName = "GetStatus";
        GetStatusFunc.ReturnTypeName = "std::string";
        GetStatusFunc.ReturnType = ReflectionType::String;
        GetStatusFunc.Flags = HFunctionFlags::BlueprintCallable | HFunctionFlags::BlueprintPure;
        GetStatusFunc.Category = "Player";
        GetStatusFunc.Invoker = [](void* Obj, const std::vector<void*>& Args) -> void* {
            static std::string result;
            result = static_cast<HPlayer*>(Obj)->GetStatus();
            return &result;
        };
        HPlayerClass.Methods.push_back(GetStatusFunc);
        
        HelianthusReflectionSystem.RegisterHClass(HPlayerClass);
        
        // 3. 查询 Helianthus 风格类型信息
        Logger::Info("3. 查询 Helianthus 风格类型信息");
        
        auto HClassNames = HelianthusReflectionSystem.GetAllHClassNames();
        Logger::Info("已注册的 Helianthus 风格类 (" + std::to_string(HClassNames.size()) + "):");
        
        for (const auto& Name : HClassNames) {
            Logger::Info("  - " + Name);
            
            const HClassInfo* Info = HelianthusReflectionSystem.GetHClassInfo(Name);
            if (Info) {
                auto PropertyNames = HelianthusReflectionSystem.GetAllHPropertyNames(Name);
                Logger::Info("    属性 (" + std::to_string(PropertyNames.size()) + "):");
                for (const auto& PropName : PropertyNames) {
                    const HPropertyInfo* PropInfo = HelianthusReflectionSystem.GetHPropertyInfo(Name, PropName);
                    if (PropInfo) {
                        Logger::Info("      - " + PropName + " (" + PropInfo->TypeName + ") [" + PropInfo->Category + "]");
                    }
                }
                
                auto FunctionNames = HelianthusReflectionSystem.GetAllHFunctionNames(Name);
                Logger::Info("    函数 (" + std::to_string(FunctionNames.size()) + "):");
                for (const auto& FuncName : FunctionNames) {
                    const HFunctionInfo* FuncInfo = HelianthusReflectionSystem.GetHFunctionInfo(Name, FuncName);
                    if (FuncInfo) {
                        Logger::Info("      - " + FuncName + " -> " + FuncInfo->ReturnTypeName + " [" + FuncInfo->Category + "]");
                    }
                }
            }
        }
        
        // 4. 创建和使用 Helianthus 风格对象
        Logger::Info("4. 创建和使用 Helianthus 风格对象");
        
        // 创建对象
        void* PlayerObj = HelianthusReflectionSystem.CreateHObject("HPlayer");
        
        if (PlayerObj) {
            Logger::Info("成功创建 Helianthus 风格对象");
            
            // 设置属性
            std::string PlayerName = "Hero";
            HelianthusReflectionSystem.SetHProperty(PlayerObj, "Name", &PlayerName);
            
            int32_t PlayerHealth = 150;
            HelianthusReflectionSystem.SetHProperty(PlayerObj, "Health", &PlayerHealth);
            
            float PlayerSpeed = 1.2f;
            HelianthusReflectionSystem.SetHProperty(PlayerObj, "Speed", &PlayerSpeed);
            
            // 调用函数
            std::vector<void*> Args;
            int32_t Damage = 30;
            Args.push_back(&Damage);
            HelianthusReflectionSystem.CallHFunction(PlayerObj, "TakeDamage", Args);
            
            // 获取属性
            void* HealthValue = HelianthusReflectionSystem.GetHProperty(PlayerObj, "Health");
            if (HealthValue) {
                int32_t CurrentHealth = *static_cast<int32_t*>(HealthValue);
                Logger::Info("Player 当前血量: " + std::to_string(CurrentHealth));
            }
            
            // 调用纯函数
            Args.clear();
            void* StatusResult = HelianthusReflectionSystem.CallHFunction(PlayerObj, "GetStatus", Args);
            if (StatusResult) {
                std::string Status = *static_cast<std::string*>(StatusResult);
                Logger::Info("Player 状态: " + Status);
            }
            
            // 销毁对象
            HelianthusReflectionSystem.DestroyHObject("HPlayer", PlayerObj);
        }
        
        // 5. 生成脚本绑定
        Logger::Info("5. 生成脚本绑定");
        
        std::string ScriptBindings = HelianthusReflectionSystem.GenerateScriptBindings("lua");
        Logger::Info("生成的脚本绑定代码长度: " + std::to_string(ScriptBindings.length()));
        
        if (!ScriptBindings.empty()) {
            Logger::Info("脚本绑定代码预览:");
            std::cout << ScriptBindings.substr(0, 500) << "..." << std::endl;
        }
        
        // 保存脚本绑定
        bool SaveResult = HelianthusReflectionSystem.SaveScriptBindings("helianthus_bindings.lua", "lua");
        if (SaveResult) {
            Logger::Info("Helianthus 风格脚本绑定已保存到 helianthus_bindings.lua");
        } else {
            Logger::Warn("保存 Helianthus 风格脚本绑定失败");
        }
        
        // 6. 演示实际对象操作
        Logger::Info("6. 演示实际对象操作");
        
        // 创建实际对象
        HPlayer Player("Hero", 150, 1.2f);
        HWeapon Weapon("MagicSword", 25, 2.5f, "Sword");
        
        Logger::Info("Player 状态: " + Player.GetStatus());
        Logger::Info("Weapon 描述: " + Weapon.GetDescription());
        
        // 模拟游戏操作
        Player.TakeDamage(30);
        Logger::Info("Player 受到 30 点伤害后: " + Player.GetStatus());
        
        Player.Heal(20);
        Logger::Info("Player 恢复 20 点血量后: " + Player.GetStatus());
        
        Weapon.Upgrade();
        Logger::Info("Weapon 升级后: " + Weapon.GetDescription());
        
        // 7. 清理
        Logger::Info("7. 清理资源");
        ShutdownHelianthusReflectionSystem();
        
        Logger::Info("=== Helianthus 风格反射系统示例完成 ===");
        
    }
    catch (const std::exception& e)
    {
        Logger::Error("Helianthus 风格示例运行出错: " + std::string(e.what()));
        return 1;
    }
    
    return 0;
}


