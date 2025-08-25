# 智能自动注册系统

## 概述

智能自动注册系统是对原有反射系统的重要改进，旨在提供更智能、更自动化的类型注册机制。该系统包含多种注册方式，从简单的宏到复杂的代码生成，满足不同场景的需求。

## 系统架构

### 1. 自动注册管理器 (AutoRegistrationManager)

自动注册管理器是系统的核心组件，负责管理所有注册器并执行注册操作。

```cpp
class AutoRegistrationManager
{
public:
    static AutoRegistrationManager& GetInstance();
    void RegisterRegistrar(std::unique_ptr<IAutoRegistrar> Registrar);
    void PerformAllRegistrations(ReflectionSystem* System);
    void Clear();
};
```

### 2. 编译时反射 (CompileTimeReflection)

使用现代C++的编译时特性进行类型信息收集和注册。

```cpp
namespace CompileTime
{
    template<typename T>
    struct TypeTraits
    {
        static constexpr bool IsClass = std::is_class_v<T>;
        static constexpr bool IsPolymorphic = std::is_polymorphic_v<T>;
        static constexpr bool IsAbstract = std::is_abstract_v<T>;
        static constexpr size_t Size = sizeof(T);
        // ... 更多类型特征
    };
}
```

### 3. 代码生成器 (CodeGenerator)

自动生成反射代码，减少手动编写的工作量。

```cpp
class CodeGenerator
{
public:
    static std::string GenerateHeader(const std::string& ClassName);
    static std::string GenerateImplementation(const std::string& ClassName, 
                                            const std::vector<std::string>& Properties,
                                            const std::vector<std::string>& Methods);
    static bool GenerateReflectionCode(const std::string& ClassName,
                                     const std::vector<std::string>& Properties,
                                     const std::vector<std::string>& Methods,
                                     const std::string& OutputDir);
};
```

## 使用方法

### 1. 智能注册宏

最简单的使用方式，只需要一行宏即可完成注册：

```cpp
class Player
{
public:
    std::string Name;
    int Health;
    float Speed;
    
    void TakeDamage(int damage) { Health -= damage; }
    void Heal(int amount) { Health += amount; }
};

// 智能注册
HELIANTHUS_SMART_REGISTER_CLASS(Player, "Name", "Health", "Speed")
HELIANTHUS_SMART_REGISTER_METHODS(Player, "TakeDamage", "Heal")
```

### 2. 编译时注册

使用编译时反射特性进行自动注册：

```cpp
class Weapon
{
public:
    std::string Type;
    int Damage;
    float Range;
    
    void Upgrade() { Damage += 5; }
    bool IsRanged() const { return Range > 2.0f; }
};

// 编译时注册
HELIANTHUS_INTELLIGENT_CLASS(Weapon, "")
```

### 3. 代码生成器

对于复杂的类，可以使用代码生成器自动生成反射代码：

```cpp
// 注册类信息
auto& SmartManager = SmartRegistrationManager::GetInstance();
SmartManager.RegisterClassInfo("Player", {"Name", "Health", "Speed"}, 
                              {"TakeDamage", "Heal", "IsAlive", "GetStatus"});

// 生成反射代码
SmartManager.GenerateAllReflectionCode("Generated");
```

### 4. 自动注册初始化

在程序启动时自动执行所有注册：

```cpp
// 初始化反射系统
auto ReflectionSystem = std::make_unique<ReflectionSystem>();
GlobalReflectionSystem = std::move(ReflectionSystem);

// 执行自动注册
AutoRegistrationInitializer::Initialize(GlobalReflectionSystem.get());
```

## 高级特性

### 1. 类型特征检测

系统可以自动检测类型的各种特征：

```cpp
using PlayerTraits = CompileTime::TypeTraits<Player>;
Logger::Info("Player类特征:");
Logger::Info("  - 是类: " + std::string(PlayerTraits::IsClass ? "是" : "否"));
Logger::Info("  - 是多态: " + std::string(PlayerTraits::IsPolymorphic ? "是" : "否"));
Logger::Info("  - 可默认构造: " + std::string(PlayerTraits::IsDefaultConstructible ? "是" : "否"));
Logger::Info("  - 大小: " + std::to_string(PlayerTraits::Size) + " 字节");
```

### 2. 自动属性检测

系统可以自动检测公共成员变量：

```cpp
#define HELIANTHUS_AUTO_PROPERTY(PropertyName) \
    HELIANTHUS_REFLECT_PROPERTY(PropertyName, decltype(PropertyName), \
        [](auto* obj) -> decltype(auto) { return obj->PropertyName; }, \
        [](auto* obj, auto value) { obj->PropertyName = value; })
```

### 3. 自动方法检测

系统可以自动检测公共方法：

```cpp
#define HELIANTHUS_AUTO_METHOD(MethodName) \
    HELIANTHUS_REFLECT_METHOD(MethodName, decltype(std::declval<decltype(this)>()->MethodName()), \
        decltype(std::declval<decltype(this)>()->MethodName()))
```

### 4. 构建系统集成

生成的代码包含构建系统配置：

```cpp
// 生成Bazel BUILD片段
std::string BazelFragment = CodeGenerator::GenerateBazelFragment("Player");

// 生成CMakeLists.txt片段
std::string CMakeFragment = CodeGenerator::GenerateCMakeFragment("Player");
```

## 优势

### 1. 减少手动工作

- 自动检测类型特征
- 自动生成反射代码
- 自动注册到反射系统

### 2. 编译时优化

- 使用编译时反射减少运行时开销
- 类型安全的注册机制
- 编译时错误检查

### 3. 灵活性

- 多种注册方式可选
- 支持自定义注册逻辑
- 可扩展的架构设计

### 4. 开发体验

- 简单的宏接口
- 自动代码生成
- 集成构建系统

## 示例

完整的示例代码请参考 `Examples/SmartReflectionExample.cpp`，该示例展示了：

1. 智能自动注册的使用
2. 编译时反射的应用
3. 代码生成器的功能
4. 自动注册初始化的过程
5. 实际对象操作的演示

## 构建和运行

```bash
# 构建智能反射示例
bazel build //Examples:smart_reflection_example

# 运行示例
bazel run //Examples:smart_reflection_example
```

## 未来改进

1. **更智能的属性检测**：使用编译器特定的技术自动检测所有公共成员
2. **模板支持**：支持模板类的自动注册
3. **继承关系自动检测**：自动检测和注册继承关系
4. **性能优化**：进一步优化编译时和运行时的性能
5. **IDE集成**：提供IDE插件支持自动代码生成

## 总结

智能自动注册系统大大简化了反射系统的使用，从需要手动编写大量注册代码，到现在只需要简单的宏或自动检测。这显著提高了开发效率，减少了出错的可能性，同时保持了系统的灵活性和可扩展性。
