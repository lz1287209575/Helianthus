# Helianthus 反射系统使用指南

## 🎯 概述

Helianthus 反射系统提供了强大的运行时类型信息查询、动态对象创建和脚本绑定功能。它支持多种使用方式，从简单的类型注册到复杂的代码生成。

## 📚 基础使用

### 1. 初始化反射系统

```cpp
#include "Shared/Reflection/ReflectionTypes.h"
#include "Shared/Reflection/ScriptBinding.h"

// 初始化反射系统
InitializeReflectionSystem();
InitializeScriptBinding();

// 使用完毕后清理
ShutdownScriptBinding();
ShutdownReflectionSystem();
```

### 2. 注册类和枚举

```cpp
// 注册一个类
ClassInfo PlayerClass;
PlayerClass.Name = "Player";
PlayerClass.FullName = "Game::Player";
PlayerClass.TypeIndex = typeid(Player);

// 添加属性
PropertyInfo HealthProp;
HealthProp.Name = "Health";
HealthProp.TypeName = "int";
HealthProp.Type = ReflectionType::Int32;
PlayerClass.Properties.push_back(HealthProp);

// 添加方法
MethodInfo HealMethod;
HealMethod.Name = "Heal";
HealMethod.ReturnTypeName = "void";
HealMethod.ReturnType = ReflectionType::Void;
PlayerClass.Methods.push_back(HealMethod);

// 注册到反射系统
GlobalReflectionSystem->RegisterClass(PlayerClass);

// 注册枚举
EnumInfo WeaponTypeEnum;
WeaponTypeEnum.Name = "WeaponType";
WeaponTypeEnum.TypeName = "int";

EnumValueInfo SwordValue;
SwordValue.Name = "Sword";
SwordValue.Value = 0;
WeaponTypeEnum.Values.push_back(SwordValue);

GlobalReflectionSystem->RegisterEnum(WeaponTypeEnum);
```

### 3. 查询类型信息

```cpp
// 获取类信息
const ClassInfo* PlayerInfo = GlobalReflectionSystem->GetClassInfo("Player");
if (PlayerInfo) {
    std::cout << "找到类: " << PlayerInfo->Name << std::endl;
    
    // 遍历属性
    for (const auto& Prop : PlayerInfo->Properties) {
        std::cout << "属性: " << Prop.Name << " (" << Prop.TypeName << ")" << std::endl;
    }
    
    // 遍历方法
    for (const auto& Method : PlayerInfo->Methods) {
        std::cout << "方法: " << Method.Name << " -> " << Method.ReturnTypeName << std::endl;
    }
}

// 获取所有类名
auto ClassNames = GlobalReflectionSystem->GetAllClassNames();
for (const auto& Name : ClassNames) {
    std::cout << "已注册类: " << Name << std::endl;
}
```

### 4. 动态对象创建

```cpp
// 创建对象
void* PlayerObj = GlobalReflectionSystem->CreateObject("Player");
if (PlayerObj) {
    std::cout << "成功创建 Player 对象" << std::endl;
    
    // 访问属性
    void* HealthValue = GlobalReflectionSystem->GetProperty(PlayerObj, "Health");
    
    // 调用方法
    std::vector<void*> Args;
    GlobalReflectionSystem->CallMethod(PlayerObj, "Heal", Args);
    
    // 销毁对象
    GlobalReflectionSystem->DestroyObject("Player", PlayerObj);
}
```

## 🔧 高级使用

### 1. 脚本绑定

```cpp
// 设置脚本引擎
auto ScriptEngine = std::make_shared<LuaScriptEngine>();
GlobalScriptBindingManager->SetScriptEngine(ScriptEngine);

// 生成绑定代码
std::string BindingCode = GlobalScriptBindingManager->GenerateBindingCode("lua");
std::cout << "生成的绑定代码:\n" << BindingCode << std::endl;

// 保存绑定代码到文件
GlobalScriptBindingManager->SaveBindingCode("bindings.lua", "lua");

// 绑定反射类型到脚本
GlobalScriptBindingManager->BindReflectionToScript();
```

### 2. 类型检查和转换

```cpp
// 检查对象类型
bool IsPlayer = GlobalReflectionSystem->IsInstanceOf(PlayerObj, "Player");

// 检查继承关系
bool IsSubclass = GlobalReflectionSystem->IsSubclassOf("Player", "GameObject");
```

### 3. 属性访问器

```cpp
// 定义属性访问器
PropertyInfo HealthProp;
HealthProp.Name = "Health";
HealthProp.TypeName = "int";
HealthProp.Getter = [](void* obj) -> void* {
    Player* player = static_cast<Player*>(obj);
    return &player->Health;
};
HealthProp.Setter = [](void* obj, void* value) {
    Player* player = static_cast<Player*>(obj);
    player->Health = *static_cast<int*>(value);
};
```

## 🎨 Helianthus 风格使用

### 1. 定义 Helianthus 类

```cpp
#include "Shared/Reflection/HelianthusReflection.h"

// 继承自 HelianthusObject
class MyPlayer : public HelianthusObject
{
public:
    std::string Name;
    int Health;
    float Speed;
    
    // 实现反射接口
    virtual const HelianthusClassInfo* GetClass() const override {
        return StaticClass();
    }
    
    static HelianthusClassInfo* StaticClass() {
        static HelianthusClassInfo* ClassInfo = nullptr;
        if (!ClassInfo) {
            ClassInfo = new HelianthusClassInfo();
            ClassInfo->Name = "MyPlayer";
            ClassInfo->FullName = "MyPlayer";
            ClassInfo->TypeIndex = std::type_index(typeid(MyPlayer));
        }
        return ClassInfo;
    }
    
    virtual const std::string& GetClassName() const override {
        static const std::string ClassName = "MyPlayer";
        return ClassName;
    }
};
```

### 2. 使用 Helianthus 管理器

```cpp
// 获取管理器实例
auto& Manager = HelianthusReflectionManager::GetInstance();

// 注册类
Manager.RegisterHelianthusClass("MyPlayer", "HelianthusObject", 
    {"Name", "Health", "Speed"},  // 属性
    {"GetName", "SetName"}        // 方法
);

// 生成代码
Manager.GenerateAllHelianthusReflectionCode("Generated");
```

## 🛠️ 实用工具

### 1. 代码生成器

```bash
# 生成反射代码（项目内置工具，默认在构建前执行）
python3 Shared/Reflection/reflection_codegen.py /path/to/src /path/to/build/generated
```

### 2. 调试工具

```cpp
// 打印所有注册的类型
void PrintAllTypes() {
    auto ClassNames = GlobalReflectionSystem->GetAllClassNames();
    std::cout << "注册的类 (" << ClassNames.size() << "):" << std::endl;
    for (const auto& Name : ClassNames) {
        std::cout << "  - " << Name << std::endl;
    }
    
    auto EnumNames = GlobalReflectionSystem->GetAllEnumNames();
    std::cout << "注册的枚举 (" << EnumNames.size() << "):" << std::endl;
    for (const auto& Name : EnumNames) {
        std::cout << "  - " << Name << std::endl;
    }
}
```

## 📘 反射标签与自动注册（快速指南）

- 业务标签写在 HMETHOD 中（`PureFunction/Math/Utility/Deprecated` 等），C++ 语义（static/virtual/inline/const/noexcept/override/final）由函数签名自动推断，不写入 Tags。
- 类标签 `NoAutoRegister`：仅跳过该类工厂自动注册，方法元信息仍会注册，便于按标签筛选。
- 全局开关 `HELIANTHUS_REFLECTION_SKIP_FACTORY_AUTO_REGISTER`（默认 OFF）：跳过所有类的工厂自动注册。
- 自动挂载 API：
  - `Helianthus::RPC::RegisterReflectedServices(IRpcServer&)`
  - `Helianthus::RPC::RegisterReflectedServices(IRpcServer&, const std::vector<std::string>& RequiredTags)`

详见：`Docs/Reflection_Tag_And_AutoRegister_Guide.md`

### 3. 性能优化

```cpp
// 缓存类信息以提高性能
class TypeCache {
private:
    std::unordered_map<std::string, const ClassInfo*> ClassCache;
    
public:
    const ClassInfo* GetClassInfo(const std::string& Name) {
        auto It = ClassCache.find(Name);
        if (It != ClassCache.end()) {
            return It->second;
        }
        
        const ClassInfo* Info = GlobalReflectionSystem->GetClassInfo(Name);
        if (Info) {
            ClassCache[Name] = Info;
        }
        return Info;
    }
};
```

## 📋 最佳实践

### 1. 错误处理

```cpp
// 总是检查返回值
const ClassInfo* Info = GlobalReflectionSystem->GetClassInfo("Player");
if (!Info) {
    std::cerr << "类 'Player' 未找到" << std::endl;
    return;
}

// 检查对象有效性
void* Obj = GlobalReflectionSystem->CreateObject("Player");
if (!Obj) {
    std::cerr << "创建对象失败" << std::endl;
    return;
}
```

### 2. 内存管理

```cpp
// 使用 RAII 管理对象生命周期
class ScopedObject {
private:
    void* Object;
    std::string ClassName;
    
public:
    ScopedObject(const std::string& name) 
        : ClassName(name), Object(GlobalReflectionSystem->CreateObject(name)) {}
    
    ~ScopedObject() {
        if (Object) {
            GlobalReflectionSystem->DestroyObject(ClassName, Object);
        }
    }
    
    void* Get() const { return Object; }
};
```

### 3. 线程安全

```cpp
// 反射系统操作需要加锁
std::mutex ReflectionMutex;

void SafeRegisterClass(const ClassInfo& Info) {
    std::lock_guard<std::mutex> Lock(ReflectionMutex);
    GlobalReflectionSystem->RegisterClass(Info);
}
```

## 🚀 集成示例

### 完整的游戏对象系统

```cpp
// 游戏对象基类
class GameObject : public HelianthusObject {
public:
    std::string Name;
    Vector3 Position;
    
    virtual void Update(float DeltaTime) = 0;
    virtual void OnCollision(GameObject* Other) = 0;
};

// 玩家类
class Player : public GameObject {
public:
    int Health;
    float Speed;
    
    void Update(float DeltaTime) override {
        // 更新逻辑
    }
    
    void OnCollision(GameObject* Other) override {
        // 碰撞处理
    }
    
    // 反射接口实现...
};

// 使用反射系统
void SetupGameObjects() {
    // 注册所有游戏对象类型
    RegisterGameObjectTypes();
    
    // 从配置文件创建对象
    auto Config = LoadGameConfig();
    for (const auto& ObjConfig : Config.Objects) {
        void* Obj = GlobalReflectionSystem->CreateObject(ObjConfig.Type);
        if (Obj) {
            // 设置属性
            GlobalReflectionSystem->SetProperty(Obj, "Name", &ObjConfig.Name);
            GlobalReflectionSystem->SetProperty(Obj, "Position", &ObjConfig.Position);
        }
    }
}
```

这个指南涵盖了反射系统的主要使用方式。根据您的具体需求，可以选择合适的使用方式。
