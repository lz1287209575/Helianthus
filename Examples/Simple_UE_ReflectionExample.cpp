#include "Shared/Reflection/Simple_UE_Reflection.h"
#include "Shared/Common/Logger.h"

#include <iostream>
#include <memory>
#include <string>

using namespace Helianthus::Reflection;
using namespace Helianthus::Common;

// 简化的 UE 风格基础对象类
class UObject
{
public:
    virtual ~UObject() = default;
    
    // 简化的属性声明
    std::string Name;
    int32_t ID;
    
    UObject() : Name("Unknown"), ID(0) {}
    UObject(const std::string& name, int32_t id) : Name(name), ID(id) {}
    
    // 简化的函数声明
    void SetName(const std::string& name) { Name = name; }
    std::string GetName() const { return Name; }
};

// 简化的 UE 风格玩家类
class UPlayer : public UObject
{
public:
    // 简化的属性声明
    int32_t Health;
    float Speed;
    bool IsAlive;
    
    UPlayer() : Health(100), Speed(1.0f), IsAlive(true) {}
    UPlayer(const std::string& name, int32_t health, float speed) 
        : UObject(name, 0), Health(health), Speed(speed), IsAlive(true) {}
    
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

// 简化的 UE 风格武器类
class UWeapon : public UObject
{
public:
    // 简化的属性声明
    int32_t Damage;
    float Range;
    std::string WeaponType;
    
    UWeapon() : Damage(10), Range(1.5f), WeaponType("Sword") {}
    UWeapon(const std::string& name, int32_t damage, float range, const std::string& type)
        : UObject(name, 0), Damage(damage), Range(range), WeaponType(type) {}
    
    // 简化的函数声明
    void Upgrade() { Damage += 5; }
    bool IsRanged() const { return Range > 2.0f; }
    
    std::string GetDescription() const {
        return Name + " (" + WeaponType + ", DMG: " + std::to_string(Damage) + ")";
    }
};

// 简化的 UE 风格枚举
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
        Logger::Info("=== 简化 UE 风格反射系统示例 ===");
        
        // 1. 初始化简化 UE 风格反射系统
        Logger::Info("1. 初始化简化 UE 风格反射系统");
        InitializeSimpleUReflectionSystem();
        
        // 2. 手动注册简化 UE 风格类（模拟宏的效果）
        Logger::Info("2. 注册简化 UE 风格类");
        
        auto& SimpleUReflectionSystem = SimpleUReflectionSystem::GetInstance();
        
        // 注册 UObject 类
        SimpleUClassInfo UObjectClass;
        UObjectClass.ClassName = "UObject";
        UObjectClass.BaseClassName = "";
        UObjectClass.TypeIndex = std::type_index(typeid(UObject));
        UObjectClass.Flags = UClassFlags::BlueprintType;
        UObjectClass.Category = "Basic";
        UObjectClass.Constructor = [](void*) -> void* { return new UObject(); };
        UObjectClass.Destructor = [](void* Obj) { delete static_cast<UObject*>(Obj); };
        
        // 添加 UObject 属性
        SimpleUPropertyInfo NameProp;
        NameProp.PropertyName = "Name";
        NameProp.TypeName = "std::string";
        NameProp.Type = ReflectionType::String;
        NameProp.Flags = UPropertyFlags::EditAnywhere;
        NameProp.Category = "Basic";
        NameProp.Getter = [](void* Obj) -> void* { return &static_cast<UObject*>(Obj)->Name; };
        NameProp.Setter = [](void* Obj, void* Value) { static_cast<UObject*>(Obj)->Name = *static_cast<std::string*>(Value); };
        UObjectClass.Properties.push_back(NameProp);
        
        SimpleUPropertyInfo IDProp;
        IDProp.PropertyName = "ID";
        IDProp.TypeName = "int32_t";
        IDProp.Type = ReflectionType::Int32;
        IDProp.Flags = UPropertyFlags::EditAnywhere;
        IDProp.Category = "Basic";
        IDProp.Getter = [](void* Obj) -> void* { return &static_cast<UObject*>(Obj)->ID; };
        IDProp.Setter = [](void* Obj, void* Value) { static_cast<UObject*>(Obj)->ID = *static_cast<int32_t*>(Value); };
        UObjectClass.Properties.push_back(IDProp);
        
        SimpleUReflectionSystem.RegisterUClass(UObjectClass);
        
        // 注册 UPlayer 类
        SimpleUClassInfo UPlayerClass;
        UPlayerClass.ClassName = "UPlayer";
        UPlayerClass.BaseClassName = "UObject";
        UPlayerClass.TypeIndex = std::type_index(typeid(UPlayer));
        UPlayerClass.Flags = UClassFlags::BlueprintType | UClassFlags::Blueprintable;
        UPlayerClass.Category = "Player";
        UPlayerClass.Constructor = [](void*) -> void* { return new UPlayer(); };
        UPlayerClass.Destructor = [](void* Obj) { delete static_cast<UPlayer*>(Obj); };
        
        // 添加 UPlayer 属性
        SimpleUPropertyInfo HealthProp;
        HealthProp.PropertyName = "Health";
        HealthProp.TypeName = "int32_t";
        HealthProp.Type = ReflectionType::Int32;
        HealthProp.Flags = UPropertyFlags::EditAnywhere | UPropertyFlags::BlueprintReadWrite;
        HealthProp.Category = "Player";
        HealthProp.Getter = [](void* Obj) -> void* { return &static_cast<UPlayer*>(Obj)->Health; };
        HealthProp.Setter = [](void* Obj, void* Value) { static_cast<UPlayer*>(Obj)->Health = *static_cast<int32_t*>(Value); };
        UPlayerClass.Properties.push_back(HealthProp);
        
        SimpleUPropertyInfo SpeedProp;
        SpeedProp.PropertyName = "Speed";
        SpeedProp.TypeName = "float";
        SpeedProp.Type = ReflectionType::Float;
        SpeedProp.Flags = UPropertyFlags::EditAnywhere | UPropertyFlags::BlueprintReadWrite;
        SpeedProp.Category = "Player";
        SpeedProp.Getter = [](void* Obj) -> void* { return &static_cast<UPlayer*>(Obj)->Speed; };
        SpeedProp.Setter = [](void* Obj, void* Value) { static_cast<UPlayer*>(Obj)->Speed = *static_cast<float*>(Value); };
        UPlayerClass.Properties.push_back(SpeedProp);
        
        SimpleUPropertyInfo IsAliveProp;
        IsAliveProp.PropertyName = "IsAlive";
        IsAliveProp.TypeName = "bool";
        IsAliveProp.Type = ReflectionType::Bool;
        IsAliveProp.Flags = UPropertyFlags::BlueprintReadOnly;
        IsAliveProp.Category = "Player";
        IsAliveProp.Getter = [](void* Obj) -> void* { return &static_cast<UPlayer*>(Obj)->IsAlive; };
        IsAliveProp.Setter = [](void* Obj, void* Value) { static_cast<UPlayer*>(Obj)->IsAlive = *static_cast<bool*>(Value); };
        UPlayerClass.Properties.push_back(IsAliveProp);
        
        // 添加 UPlayer 函数
        SimpleUFunctionInfo TakeDamageFunc;
        TakeDamageFunc.FunctionName = "TakeDamage";
        TakeDamageFunc.ReturnTypeName = "void";
        TakeDamageFunc.ReturnType = ReflectionType::Void;
        TakeDamageFunc.Flags = UFunctionFlags::BlueprintCallable;
        TakeDamageFunc.Category = "Player";
        TakeDamageFunc.Invoker = [](void* Obj, const std::vector<void*>& Args) -> void* {
            if (Args.size() >= 1) {
                int32_t damage = *static_cast<int32_t*>(Args[0]);
                static_cast<UPlayer*>(Obj)->TakeDamage(damage);
            }
            return nullptr;
        };
        UPlayerClass.Methods.push_back(TakeDamageFunc);
        
        SimpleUFunctionInfo HealFunc;
        HealFunc.FunctionName = "Heal";
        HealFunc.ReturnTypeName = "void";
        HealFunc.ReturnType = ReflectionType::Void;
        HealFunc.Flags = UFunctionFlags::BlueprintCallable;
        HealFunc.Category = "Player";
        HealFunc.Invoker = [](void* Obj, const std::vector<void*>& Args) -> void* {
            if (Args.size() >= 1) {
                int32_t amount = *static_cast<int32_t*>(Args[0]);
                static_cast<UPlayer*>(Obj)->Heal(amount);
            }
            return nullptr;
        };
        UPlayerClass.Methods.push_back(HealFunc);
        
        SimpleUFunctionInfo GetStatusFunc;
        GetStatusFunc.FunctionName = "GetStatus";
        GetStatusFunc.ReturnTypeName = "std::string";
        GetStatusFunc.ReturnType = ReflectionType::String;
        GetStatusFunc.Flags = UFunctionFlags::BlueprintCallable | UFunctionFlags::BlueprintPure;
        GetStatusFunc.Category = "Player";
        GetStatusFunc.Invoker = [](void* Obj, const std::vector<void*>& Args) -> void* {
            static std::string result;
            result = static_cast<UPlayer*>(Obj)->GetStatus();
            return &result;
        };
        UPlayerClass.Methods.push_back(GetStatusFunc);
        
        SimpleUReflectionSystem.RegisterUClass(UPlayerClass);
        
        // 3. 查询简化 UE 风格类型信息
        Logger::Info("3. 查询简化 UE 风格类型信息");
        
        auto UClassNames = SimpleUReflectionSystem.GetAllUClassNames();
        Logger::Info("已注册的简化 UE 风格类 (" + std::to_string(UClassNames.size()) + "):");
        
        for (const auto& Name : UClassNames) {
            Logger::Info("  - " + Name);
            
            const SimpleUClassInfo* Info = SimpleUReflectionSystem.GetUClassInfo(Name);
            if (Info) {
                auto PropertyNames = SimpleUReflectionSystem.GetAllUPropertyNames(Name);
                Logger::Info("    属性 (" + std::to_string(PropertyNames.size()) + "):");
                for (const auto& PropName : PropertyNames) {
                    const SimpleUPropertyInfo* PropInfo = SimpleUReflectionSystem.GetUPropertyInfo(Name, PropName);
                    if (PropInfo) {
                        Logger::Info("      - " + PropName + " (" + PropInfo->TypeName + ") [" + PropInfo->Category + "]");
                    }
                }
                
                auto FunctionNames = SimpleUReflectionSystem.GetAllUFunctionNames(Name);
                Logger::Info("    函数 (" + std::to_string(FunctionNames.size()) + "):");
                for (const auto& FuncName : FunctionNames) {
                    const SimpleUFunctionInfo* FuncInfo = SimpleUReflectionSystem.GetUFunctionInfo(Name, FuncName);
                    if (FuncInfo) {
                        Logger::Info("      - " + FuncName + " -> " + FuncInfo->ReturnTypeName + " [" + FuncInfo->Category + "]");
                    }
                }
            }
        }
        
        // 4. 创建和使用简化 UE 风格对象
        Logger::Info("4. 创建和使用简化 UE 风格对象");
        
        // 创建对象
        void* PlayerObj = SimpleUReflectionSystem.CreateUObject("UPlayer");
        
        if (PlayerObj) {
            Logger::Info("成功创建简化 UE 风格对象");
            
            // 设置属性
            std::string PlayerName = "Hero";
            SimpleUReflectionSystem.SetUProperty(PlayerObj, "Name", &PlayerName);
            
            int32_t PlayerHealth = 150;
            SimpleUReflectionSystem.SetUProperty(PlayerObj, "Health", &PlayerHealth);
            
            float PlayerSpeed = 1.2f;
            SimpleUReflectionSystem.SetUProperty(PlayerObj, "Speed", &PlayerSpeed);
            
            // 调用函数
            std::vector<void*> Args;
            int32_t Damage = 30;
            Args.push_back(&Damage);
            SimpleUReflectionSystem.CallUFunction(PlayerObj, "TakeDamage", Args);
            
            // 获取属性
            void* HealthValue = SimpleUReflectionSystem.GetUProperty(PlayerObj, "Health");
            if (HealthValue) {
                int32_t CurrentHealth = *static_cast<int32_t*>(HealthValue);
                Logger::Info("Player 当前血量: " + std::to_string(CurrentHealth));
            }
            
            // 调用纯函数
            Args.clear();
            void* StatusResult = SimpleUReflectionSystem.CallUFunction(PlayerObj, "GetStatus", Args);
            if (StatusResult) {
                std::string Status = *static_cast<std::string*>(StatusResult);
                Logger::Info("Player 状态: " + Status);
            }
            
            // 销毁对象
            SimpleUReflectionSystem.DestroyUObject("UPlayer", PlayerObj);
        }
        
        // 5. 生成脚本绑定
        Logger::Info("5. 生成脚本绑定");
        
        std::string ScriptBindings = SimpleUReflectionSystem.GenerateScriptBindings("lua");
        Logger::Info("生成的脚本绑定代码长度: " + std::to_string(ScriptBindings.length()));
        
        if (!ScriptBindings.empty()) {
            Logger::Info("脚本绑定代码预览:");
            std::cout << ScriptBindings.substr(0, 500) << "..." << std::endl;
        }
        
        // 保存脚本绑定
        bool SaveResult = SimpleUReflectionSystem.SaveScriptBindings("simple_ue_bindings.lua", "lua");
        if (SaveResult) {
            Logger::Info("简化 UE 风格脚本绑定已保存到 simple_ue_bindings.lua");
        } else {
            Logger::Warn("保存简化 UE 风格脚本绑定失败");
        }
        
        // 6. 演示实际对象操作
        Logger::Info("6. 演示实际对象操作");
        
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
        
        // 7. 清理
        Logger::Info("7. 清理资源");
        ShutdownSimpleUReflectionSystem();
        
        Logger::Info("=== 简化 UE 风格反射系统示例完成 ===");
        
    }
    catch (const std::exception& e)
    {
        Logger::Error("简化 UE 风格示例运行出错: " + std::string(e.what()));
        return 1;
    }
    
    return 0;
}


