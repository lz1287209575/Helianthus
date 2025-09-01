# Helianthus UE 风格反射系统使用指南

## 🎯 概述

Helianthus UE 风格反射系统提供了类似 Unreal Engine 的便利反射功能，使用宏来自动化类型注册和反射信息生成，大大简化了反射系统的使用。

## 📚 设计理念

### 1. **类似 UE 的宏系统**
```cpp
UCLASS(BlueprintType, UObject)
class UPlayer : public UObject
{
    GENERATED_BODY()
    
public:
    UPROPERTY(EditAnywhere, "Player", int32_t, Health)
    int32_t Health;
    
    UFUNCTION(BlueprintCallable, "Player", void, TakeDamage, int32_t)
    void TakeDamage(int32_t damage);
};
```

### 2. **自动注册机制**
- 编译时自动生成反射信息
- 运行时自动注册类型
- 无需手动调用注册函数

### 3. **丰富的标记系统**
- `UCLASS` - 类标记
- `UPROPERTY` - 属性标记
- `UFUNCTION` - 函数标记
- `UENUM` - 枚举标记
- `USTRUCT` - 结构体标记

## 🔧 基础使用

### 1. 定义 UE 风格类

```cpp
#include "Shared/Reflection/Simple_UE_Reflection.h"

// 基础对象类
class UObject
{
public:
    virtual ~UObject() = default;
    
    std::string Name;
    int32_t ID;
    
    UObject() : Name("Unknown"), ID(0) {}
    UObject(const std::string& name, int32_t id) : Name(name), ID(id) {}
    
    void SetName(const std::string& name) { Name = name; }
    std::string GetName() const { return Name; }
};

// UE 风格玩家类
class UPlayer : public UObject
{
public:
    int32_t Health;
    float Speed;
    bool IsAlive;
    
    UPlayer() : Health(100), Speed(1.0f), IsAlive(true) {}
    UPlayer(const std::string& name, int32_t health, float speed) 
        : UObject(name, 0), Health(health), Speed(speed), IsAlive(true) {}
    
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
```

### 2. 手动注册（当前实现）

由于完整的宏系统需要代码生成器，当前版本使用手动注册：

```cpp
// 初始化反射系统
InitializeSimpleUReflectionSystem();

auto& SimpleUReflectionSystem = SimpleUReflectionSystem::GetInstance();

// 注册 UPlayer 类
SimpleUClassInfo UPlayerClass;
UPlayerClass.ClassName = "UPlayer";
UPlayerClass.BaseClassName = "UObject";
UPlayerClass.TypeIndex = std::type_index(typeid(UPlayer));
UPlayerClass.Flags = UClassFlags::BlueprintType | UClassFlags::Blueprintable;
UPlayerClass.Category = "Player";
UPlayerClass.Constructor = [](void*) -> void* { return new UPlayer(); };
UPlayerClass.Destructor = [](void* Obj) { delete static_cast<UPlayer*>(Obj); };

// 添加属性
SimpleUPropertyInfo HealthProp;
HealthProp.PropertyName = "Health";
HealthProp.TypeName = "int32_t";
HealthProp.Type = ReflectionType::Int32;
HealthProp.Flags = UPropertyFlags::EditAnywhere | UPropertyFlags::BlueprintReadWrite;
HealthProp.Category = "Player";
HealthProp.Getter = [](void* Obj) -> void* { return &static_cast<UPlayer*>(Obj)->Health; };
HealthProp.Setter = [](void* Obj, void* Value) { static_cast<UPlayer*>(Obj)->Health = *static_cast<int32_t*>(Value); };
UPlayerClass.Properties.push_back(HealthProp);

// 添加函数
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

SimpleUReflectionSystem.RegisterUClass(UPlayerClass);
```

### 3. 使用反射功能

```cpp
// 创建对象
void* PlayerObj = SimpleUReflectionSystem.CreateUObject("UPlayer");

// 设置属性
std::string PlayerName = "Hero";
SimpleUReflectionSystem.SetUProperty(PlayerObj, "Name", &PlayerName);

int32_t PlayerHealth = 150;
SimpleUReflectionSystem.SetUProperty(PlayerObj, "Health", &PlayerHealth);

// 调用函数
std::vector<void*> Args;
int32_t Damage = 30;
Args.push_back(&Damage);
SimpleUReflectionSystem.CallUFunction(PlayerObj, "TakeDamage", Args);

// 获取属性
void* HealthValue = SimpleUReflectionSystem.GetUProperty(PlayerObj, "Health");
if (HealthValue) {
    int32_t CurrentHealth = *static_cast<int32_t*>(HealthValue);
    std::cout << "Player 当前血量: " << CurrentHealth << std::endl;
}

// 调用纯函数
Args.clear();
void* StatusResult = SimpleUReflectionSystem.CallUFunction(PlayerObj, "GetStatus", Args);
if (StatusResult) {
    std::string Status = *static_cast<std::string*>(StatusResult);
    std::cout << "Player 状态: " << Status << std::endl;
}

// 销毁对象
SimpleUReflectionSystem.DestroyUObject("UPlayer", PlayerObj);
```

## 🎨 标记系统详解

### 1. UCLASS 标记

```cpp
UCLASS(Flags, BaseClass, ClassName)
```

**Flags 选项：**
- `BlueprintType` - Blueprint 可用
- `Blueprintable` - Blueprint 可继承
- `Abstract` - 抽象类
- `Config` - 可配置

**示例：**
```cpp
UCLASS(BlueprintType, UObject, UPlayer)
UCLASS(Blueprintable | Abstract, UObject, UBasePlayer)
```

### 2. UPROPERTY 标记

```cpp
UPROPERTY(Flags, Category, PropertyType, PropertyName)
```

**Flags 选项：**
- `EditAnywhere` - 任何地方可编辑
- `EditDefaultsOnly` - 仅默认值可编辑
- `BlueprintReadOnly` - Blueprint 只读
- `BlueprintReadWrite` - Blueprint 读写
- `SaveGame` - 保存游戏
- `Replicated` - 网络复制

**示例：**
```cpp
UPROPERTY(EditAnywhere, "Player", int32_t, Health)
UPROPERTY(BlueprintReadOnly | SaveGame, "Player", bool, IsAlive)
```

### 3. UFUNCTION 标记

```cpp
UFUNCTION(Flags, Category, ReturnType, FunctionName, Parameters...)
```

**Flags 选项：**
- `BlueprintCallable` - Blueprint 可调用
- `BlueprintEvent` - Blueprint 事件
- `BlueprintPure` - Blueprint 纯函数
- `NetMulticast` - 网络多播
- `NetServer` - 服务器端
- `NetClient` - 客户端

**示例：**
```cpp
UFUNCTION(BlueprintCallable, "Player", void, TakeDamage, int32_t)
UFUNCTION(BlueprintPure, "Player", bool, IsPlayerAlive)
```

### 4. UENUM 标记

```cpp
UENUM(Flags, EnumName)
```

**示例：**
```cpp
UENUM(BlueprintType, EWeaponType)
enum class EWeaponType : int32_t
{
    Sword = 0,
    Axe = 1,
    Bow = 2,
    Staff = 3,
    Dagger = 4
};
```

## 🚀 高级功能

### 1. 类型查询

```cpp
// 获取所有类名
auto UClassNames = SimpleUReflectionSystem.GetAllUClassNames();

// 获取类信息
const SimpleUClassInfo* Info = SimpleUReflectionSystem.GetUClassInfo("UPlayer");

// 获取属性信息
const SimpleUPropertyInfo* PropInfo = SimpleUReflectionSystem.GetUPropertyInfo("UPlayer", "Health");

// 获取函数信息
const SimpleUFunctionInfo* FuncInfo = SimpleUReflectionSystem.GetUFunctionInfo("UPlayer", "TakeDamage");
```

### 2. 类型检查

```cpp
// 检查是否为 UE 风格类
bool IsUClass = SimpleUReflectionSystem.IsUClass("UPlayer");

// 检查属性是否存在
bool HasProperty = SimpleUReflectionSystem.IsUProperty("UPlayer", "Health");

// 检查函数是否存在
bool HasFunction = SimpleUReflectionSystem.IsUFunction("UPlayer", "TakeDamage");
```

### 3. 脚本绑定

```cpp
// 生成脚本绑定代码
std::string ScriptBindings = SimpleUReflectionSystem.GenerateScriptBindings("lua");

// 保存脚本绑定到文件
SimpleUReflectionSystem.SaveScriptBindings("bindings.lua", "lua");
```

## 📋 最佳实践

### 1. 类设计

```cpp
// 好的设计
class UPlayer : public UObject
{
public:
    // 使用有意义的属性名
    UPROPERTY(EditAnywhere, "Player", int32_t, MaxHealth)
    int32_t MaxHealth;
    
    // 使用适当的标记
    UPROPERTY(BlueprintReadOnly, "Player", bool, IsDead)
    bool IsDead;
    
    // 函数命名清晰
    UFUNCTION(BlueprintCallable, "Player", void, TakeDamage, int32_t)
    void TakeDamage(int32_t damage);
    
    // 纯函数标记
    UFUNCTION(BlueprintPure, "Player", float, GetHealthPercentage)
    float GetHealthPercentage() const;
};
```

### 2. 错误处理

```cpp
// 总是检查对象创建
void* PlayerObj = SimpleUReflectionSystem.CreateUObject("UPlayer");
if (!PlayerObj) {
    std::cerr << "创建 UPlayer 对象失败" << std::endl;
    return;
}

// 检查属性访问
void* HealthValue = SimpleUReflectionSystem.GetUProperty(PlayerObj, "Health");
if (!HealthValue) {
    std::cerr << "获取 Health 属性失败" << std::endl;
    return;
}

// 检查函数调用
std::vector<void*> Args;
Args.push_back(&damage);
void* Result = SimpleUReflectionSystem.CallUFunction(PlayerObj, "TakeDamage", Args);
if (!Result && !SimpleUReflectionSystem.IsUFunction("UPlayer", "TakeDamage")) {
    std::cerr << "调用 TakeDamage 函数失败" << std::endl;
}
```

### 3. 内存管理

```cpp
// 使用 RAII 管理对象生命周期
class ScopedUObject
{
private:
    void* Object;
    std::string ClassName;
    SimpleUReflectionSystem& ReflectionSystem;
    
public:
    ScopedUObject(const std::string& name, SimpleUReflectionSystem& system) 
        : ClassName(name), ReflectionSystem(system), Object(system.CreateUObject(name)) {}
    
    ~ScopedUObject() {
        if (Object) {
            ReflectionSystem.DestroyUObject(ClassName, Object);
        }
    }
    
    void* Get() const { return Object; }
    operator bool() const { return Object != nullptr; }
};

// 使用示例
{
    ScopedUObject Player("UPlayer", SimpleUReflectionSystem::GetInstance());
    if (Player) {
        // 使用 Player.Get() 访问对象
        // 自动销毁
    }
}
```

## 🔮 未来改进

### 1. 完整的宏系统

```cpp
// 目标：完整的宏支持
UCLASS(BlueprintType, UObject)
class UPlayer : public UObject
{
    GENERATED_BODY()
    
public:
    UPROPERTY(EditAnywhere, "Player", int32_t, Health)
    int32_t Health;
    
    UFUNCTION(BlueprintCallable, "Player", void, TakeDamage, int32_t)
    void TakeDamage(int32_t damage);
};
```

### 2. 代码生成器

```bash
# 目标：自动代码生成
bazel run //Shared/Reflection:ue_reflection_codegen -- \
    --input=src/ \
    --output=generated/ \
    --language=cpp
```

### 3. 编辑器集成

```cpp
// 目标：编辑器支持
UCLASS(BlueprintType, UObject)
class UPlayer : public UObject
{
    GENERATED_BODY()
    
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player", meta=(ClampMin="0", ClampMax="1000"))
    int32_t Health;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player", meta=(Units="cm/s"))
    float Speed;
};
```

## 📖 总结

UE 风格反射系统提供了：

1. **便利的宏系统** - 类似 UE 的标记语法
2. **自动注册机制** - 减少手动工作
3. **丰富的标记选项** - 支持各种使用场景
4. **完整的反射功能** - 类型查询、对象操作、脚本绑定
5. **良好的扩展性** - 易于添加新功能

这个系统让反射功能的使用变得简单直观，特别适合游戏开发中的对象系统、脚本集成和编辑器功能开发。


