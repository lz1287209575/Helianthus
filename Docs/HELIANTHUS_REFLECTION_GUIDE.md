# Helianthus 风格反射系统使用指南

## 🎯 概述

Helianthus 风格反射系统提供了完全独立的反射功能，使用 `HCLASS`、`HPROPERTY`、`HFUNCTION` 等标记，避免了与 UE 相关的命名，为 Helianthus 项目提供专属的反射解决方案。

## 📚 设计理念

### 1. **完全独立的命名系统**
```cpp
HCLASS(Scriptable, HObject)
class HPlayer : public HObject
{
    GENERATED_BODY()
    
public:
    HPROPERTY(EditAnywhere, "Player", int32_t, Health)
    int32_t Health;
    
    HFUNCTION(BlueprintCallable, "Player", void, TakeDamage, int32_t)
    void TakeDamage(int32_t damage);
};
```

### 2. **Helianthus 专属的标记**
- `HCLASS` - 类标记
- `HPROPERTY` - 属性标记
- `HFUNCTION` - 函数标记
- `HENUM` - 枚举标记
- `HSTRUCT` - 结构体标记

### 3. **自动注册机制**
- 编译时自动生成反射信息
- 运行时自动注册类型
- 无需手动调用注册函数

## 🔧 基础使用

### 1. 定义 Helianthus 风格类

```cpp
#include "Shared/Reflection/Helianthus_Reflection.h"

// 基础对象类
class HObject
{
public:
    virtual ~HObject() = default;
    
    std::string Name;
    int32_t ID;
    
    HObject() : Name("Unknown"), ID(0) {}
    HObject(const std::string& name, int32_t id) : Name(name), ID(id) {}
    
    void SetName(const std::string& name) { Name = name; }
    std::string GetName() const { return Name; }
};

// Helianthus 风格玩家类
class HPlayer : public HObject
{
public:
    int32_t Health;
    float Speed;
    bool IsAlive;
    
    HPlayer() : Health(100), Speed(1.0f), IsAlive(true) {}
    HPlayer(const std::string& name, int32_t health, float speed) 
        : HObject(name, 0), Health(health), Speed(speed), IsAlive(true) {}
    
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
InitializeHelianthusReflectionSystem();

auto& HelianthusReflectionSystem = HelianthusReflectionSystem::GetInstance();

// 注册 HPlayer 类
HClassInfo HPlayerClass;
HPlayerClass.ClassName = "HPlayer";
HPlayerClass.BaseClassName = "HObject";
HPlayerClass.TypeIndex = std::type_index(typeid(HPlayer));
HPlayerClass.Flags = HClassFlags::Scriptable | HClassFlags::BlueprintType;
HPlayerClass.Categories.push_back("Player");
HPlayerClass.Constructor = [](void*) -> void* { return new HPlayer(); };
HPlayerClass.Destructor = [](void* Obj) { delete static_cast<HPlayer*>(Obj); };

// 添加属性
HPropertyInfo HealthProp;
HealthProp.PropertyName = "Health";
HealthProp.TypeName = "int32_t";
HealthProp.Type = ReflectionType::Int32;
HealthProp.Flags = HPropertyFlags::EditAnywhere | HPropertyFlags::BlueprintReadWrite;
HealthProp.Category = "Player";
HealthProp.Getter = [](void* Obj) -> void* { return &static_cast<HPlayer*>(Obj)->Health; };
HealthProp.Setter = [](void* Obj, void* Value) { static_cast<HPlayer*>(Obj)->Health = *static_cast<int32_t*>(Value); };
HPlayerClass.Properties.push_back(HealthProp);

// 添加函数
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

HelianthusReflectionSystem.RegisterHClass(HPlayerClass);
```

### 3. 使用反射功能

```cpp
// 创建对象
void* PlayerObj = HelianthusReflectionSystem.CreateHObject("HPlayer");

// 设置属性
std::string PlayerName = "Hero";
HelianthusReflectionSystem.SetHProperty(PlayerObj, "Name", &PlayerName);

int32_t PlayerHealth = 150;
HelianthusReflectionSystem.SetHProperty(PlayerObj, "Health", &PlayerHealth);

// 调用函数
std::vector<void*> Args;
int32_t Damage = 30;
Args.push_back(&Damage);
HelianthusReflectionSystem.CallHFunction(PlayerObj, "TakeDamage", Args);

// 获取属性
void* HealthValue = HelianthusReflectionSystem.GetHProperty(PlayerObj, "Health");
if (HealthValue) {
    int32_t CurrentHealth = *static_cast<int32_t*>(HealthValue);
    std::cout << "Player 当前血量: " << CurrentHealth << std::endl;
}

// 调用纯函数
Args.clear();
void* StatusResult = HelianthusReflectionSystem.CallHFunction(PlayerObj, "GetStatus", Args);
if (StatusResult) {
    std::string Status = *static_cast<std::string*>(StatusResult);
    std::cout << "Player 状态: " << Status << std::endl;
}

// 销毁对象
HelianthusReflectionSystem.DestroyHObject("HPlayer", PlayerObj);
```

## 🎨 标记系统详解

### 1. HCLASS 标记

```cpp
HCLASS(Flags, BaseClass, ClassName)
```

**Flags 选项：**
- `Scriptable` - 脚本可访问
- `BlueprintType` - Blueprint 可用
- `Blueprintable` - Blueprint 可继承
- `Abstract` - 抽象类
- `Config` - 可配置
- `DefaultToInstanced` - 默认实例化
- `Transient` - 临时对象
- `NotPlaceable` - 不可放置
- `AdvancedDisplay` - 高级显示
- `Hidden` - 隐藏
- `Deprecated` - 已弃用
- `HideDropdown` - 隐藏下拉菜单
- `GlobalUserConfig` - 全局用户配置
- `AutoExpandCategories` - 自动展开分类
- `AutoCollapseCategories` - 自动折叠分类

**示例：**
```cpp
HCLASS(Scriptable, HObject, HPlayer)
HCLASS(BlueprintType | Blueprintable, HObject, HBasePlayer)
```

### 2. HPROPERTY 标记

```cpp
HPROPERTY(Flags, Category, PropertyType, PropertyName)
```

**Flags 选项：**
- `Edit` - 可编辑
- `EditConst` - 编辑时只读
- `EditAnywhere` - 任何地方可编辑
- `EditDefaultsOnly` - 仅默认值可编辑
- `EditInstanceOnly` - 仅实例可编辑
- `BlueprintReadOnly` - Blueprint 只读
- `BlueprintReadWrite` - Blueprint 读写
- `BlueprintAssignable` - Blueprint 可分配
- `BlueprintCallable` - Blueprint 可调用
- `BlueprintAuthorityOnly` - 仅权威端
- `BlueprintCosmetic` - Blueprint 装饰性
- `SaveGame` - 保存游戏
- `Replicated` - 网络复制
- `ReplicatedUsing` - 使用函数复制
- `ReplicatedFrom` - 从某处复制
- `Interp` - 插值
- `NonTransactional` - 非事务性
- `InstancedReference` - 实例引用
- `SkipSerialization` - 跳过序列化
- `VisibleAnywhere` - 任何地方可见
- `VisibleInstanceOnly` - 仅实例可见
- `VisibleDefaultsOnly` - 仅默认值可见
- `AdvancedDisplay` - 高级显示
- `Hidden` - 隐藏
- `Transient` - 临时
- `Config` - 配置
- `GlobalConfig` - 全局配置
- `Localized` - 本地化
- `DuplicateTransient` - 复制时临时
- `TextExportTransient` - 文本导出时临时
- `NonPIEDuplicateTransient` - 非 PIE 复制时临时
- `ExportObject` - 导出对象

**示例：**
```cpp
HPROPERTY(EditAnywhere, "Player", int32_t, Health)
HPROPERTY(BlueprintReadOnly | SaveGame, "Player", bool, IsAlive)
```

### 3. HFUNCTION 标记

```cpp
HFUNCTION(Flags, Category, ReturnType, FunctionName, Parameters...)
```

**Flags 选项：**
- `Final` - 最终函数
- `RequiredAPI` - 必需 API
- `BlueprintAuthorityOnly` - 仅权威端
- `BlueprintCosmetic` - 装饰性
- `BlueprintCallable` - Blueprint 可调用
- `BlueprintEvent` - Blueprint 事件
- `BlueprintPure` - Blueprint 纯函数
- `BlueprintImplementableEvent` - Blueprint 可实现事件
- `BlueprintNativeEvent` - Blueprint 原生事件
- `Public` - 公共
- `Private` - 私有
- `Protected` - 保护
- `Static` - 静态
- `Const` - 常量
- `Exec` - 可执行
- `HasOutParms` - 有输出参数
- `HasDefaults` - 有默认值
- `NetClient` - 网络客户端
- `DLLImport` - DLL 导入
- `DLLExport` - DLL 导出
- `NetServer` - 网络服务器
- `HasOptionalParms` - 有可选参数
- `NetReliable` - 网络可靠
- `Simulated` - 模拟
- `ExecWhen` - 执行时机
- `Event` - 事件
- `NetResponse` - 网络响应
- `StaticConversion` - 静态转换
- `MulticastDelegate` - 多播委托
- `MulticastInline` - 多播内联
- `MulticastSparse` - 多播稀疏
- `MulticastReliable` - 多播可靠

**示例：**
```cpp
HFUNCTION(BlueprintCallable, "Player", void, TakeDamage, int32_t)
HFUNCTION(BlueprintPure, "Player", bool, IsPlayerAlive)
```

### 4. HENUM 标记

```cpp
HENUM(Flags, EnumName)
```

**示例：**
```cpp
HENUM(BlueprintType, HWeaponType)
enum class HWeaponType : int32_t
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
auto HClassNames = HelianthusReflectionSystem.GetAllHClassNames();

// 获取类信息
const HClassInfo* Info = HelianthusReflectionSystem.GetHClassInfo("HPlayer");

// 获取属性信息
const HPropertyInfo* PropInfo = HelianthusReflectionSystem.GetHPropertyInfo("HPlayer", "Health");

// 获取函数信息
const HFunctionInfo* FuncInfo = HelianthusReflectionSystem.GetHFunctionInfo("HPlayer", "TakeDamage");
```

### 2. 类型检查

```cpp
// 检查是否为 Helianthus 风格类
bool IsHClass = HelianthusReflectionSystem.IsHClass("HPlayer");

// 检查属性是否存在
bool HasProperty = HelianthusReflectionSystem.IsHProperty("HPlayer", "Health");

// 检查函数是否存在
bool HasFunction = HelianthusReflectionSystem.IsHFunction("HPlayer", "TakeDamage");
```

### 3. 脚本绑定

```cpp
// 生成脚本绑定代码
std::string ScriptBindings = HelianthusReflectionSystem.GenerateScriptBindings("lua");

// 保存脚本绑定到文件
HelianthusReflectionSystem.SaveScriptBindings("bindings.lua", "lua");
```

## 📋 最佳实践

### 1. 类设计

```cpp
// 好的设计
class HPlayer : public HObject
{
public:
    // 使用有意义的属性名
    HPROPERTY(EditAnywhere, "Player", int32_t, MaxHealth)
    int32_t MaxHealth;
    
    // 使用适当的标记
    HPROPERTY(BlueprintReadOnly, "Player", bool, IsDead)
    bool IsDead;
    
    // 函数命名清晰
    HFUNCTION(BlueprintCallable, "Player", void, TakeDamage, int32_t)
    void TakeDamage(int32_t damage);
    
    // 纯函数标记
    HFUNCTION(BlueprintPure, "Player", float, GetHealthPercentage)
    float GetHealthPercentage() const;
};
```

### 2. 错误处理

```cpp
// 总是检查对象创建
void* PlayerObj = HelianthusReflectionSystem.CreateHObject("HPlayer");
if (!PlayerObj) {
    std::cerr << "创建 HPlayer 对象失败" << std::endl;
    return;
}

// 检查属性访问
void* HealthValue = HelianthusReflectionSystem.GetHProperty(PlayerObj, "Health");
if (!HealthValue) {
    std::cerr << "获取 Health 属性失败" << std::endl;
    return;
}

// 检查函数调用
std::vector<void*> Args;
Args.push_back(&damage);
void* Result = HelianthusReflectionSystem.CallHFunction(PlayerObj, "TakeDamage", Args);
if (!Result && !HelianthusReflectionSystem.IsHFunction("HPlayer", "TakeDamage")) {
    std::cerr << "调用 TakeDamage 函数失败" << std::endl;
}
```

### 3. 内存管理

```cpp
// 使用 RAII 管理对象生命周期
class ScopedHObject
{
private:
    void* Object;
    std::string ClassName;
    HelianthusReflectionSystem& ReflectionSystem;
    
public:
    ScopedHObject(const std::string& name, HelianthusReflectionSystem& system) 
        : ClassName(name), ReflectionSystem(system), Object(system.CreateHObject(name)) {}
    
    ~ScopedHObject() {
        if (Object) {
            ReflectionSystem.DestroyHObject(ClassName, Object);
        }
    }
    
    void* Get() const { return Object; }
    operator bool() const { return Object != nullptr; }
};

// 使用示例
{
    ScopedHObject Player("HPlayer", HelianthusReflectionSystem::GetInstance());
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
HCLASS(Scriptable, HObject)
class HPlayer : public HObject
{
    GENERATED_BODY()
    
public:
    HPROPERTY(EditAnywhere, "Player", int32_t, Health)
    int32_t Health;
    
    HFUNCTION(BlueprintCallable, "Player", void, TakeDamage, int32_t)
    void TakeDamage(int32_t damage);
};
```

### 2. 代码生成器

```bash
# 目标：自动代码生成
bazel run //Shared/Reflection:helianthus_reflection_codegen -- \
    --input=src/ \
    --output=generated/ \
    --language=cpp
```

### 3. 编辑器集成

```cpp
// 目标：编辑器支持
HCLASS(Scriptable, HObject)
class HPlayer : public HObject
{
    GENERATED_BODY()
    
public:
    HPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player", meta=(ClampMin="0", ClampMax="1000"))
    int32_t Health;
    
    HPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player", meta=(Units="cm/s"))
    float Speed;
};
```

## 📖 总结

Helianthus 风格反射系统提供了：

1. **完全独立的命名** - 避免与 UE 相关的命名冲突
2. **便利的宏系统** - 类似 UE 的标记语法，但完全独立
3. **自动注册机制** - 减少手动工作
4. **丰富的标记选项** - 支持各种使用场景
5. **完整的反射功能** - 类型查询、对象操作、脚本绑定
6. **良好的扩展性** - 易于添加新功能

这个系统为 Helianthus 项目提供了专属的反射解决方案，既保持了便利性，又确保了独立性。


