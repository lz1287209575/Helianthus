# Helianthus 反射系统

## 🎯 使用方式（完全匹配您的需求）

### 1. 定义类
```cpp
// Player.h
#include "Player.GEN.h"

HCLASS(Scriptable)
class Player : public HObject
{
public:
    HPROPERTY(ScriptReadable)
    int Level = 1;

    HPROPERTY(ScriptReadable)
    int Exp = 0;

    HPROPERTY(ScriptReadable | BlueprintReadWrite)
    int Gold = 100;

    HFUNCTION(ScriptCallable)
    void OnLevelUp()
    {
        Level++;
        Exp = 0;
    }

    HFUNCTION(ScriptCallable)
    void AddExperience(int amount);
};
```

### 2. 自动生成代码
```bash
# 运行代码生成器
bazel run //Shared/Reflection:reflection_codegen -- \
    --input=Examples/ \
    --output=Examples/Generated/
```

### 3. 使用反射
```cpp
// 自动生成的 Player.GEN.h
#pragma once
#include "Player.h"

namespace Helianthus::Reflection
{
    template<>
    struct ClassInfo<Player>
    {
        static constexpr const char* Name = "Player";
        
        struct PropertyInfo
        {
            const char* Name;
            const char* Type;
            size_t Offset;
            const char* Flags;
        };
        
        static constexpr PropertyInfo Properties[] = {
            { "Level", "int", offsetof(Player, Level), "ScriptReadable" },
            { "Exp", "int", offsetof(Player, Exp), "ScriptReadable" },
            { "Gold", "int", offsetof(Player, Gold), "ScriptReadable|BlueprintReadWrite" }
        };
        
        static constexpr size_t PropertyCount = 3;
    };
}
```

### 4. 运行时访问
```cpp
// 获取属性信息
const auto* propInfo = Reflection::GetProperty("Player", "Level");
if (propInfo)
{
    std::cout << "Property: " << propInfo->Name << " (" << propInfo->Type << ")";
}

// 动态创建对象
Player* player = Reflection::CreateObject<Player>();
player->AddExperience(100);
```

## 🚀 支持的标记

### HCLASS标记
- `Scriptable` - 脚本可访问
- `BlueprintType` - Blueprint可用
- `Config` - 可配置

### HPROPERTY标记
- `ScriptReadable` - 脚本可读
- `ScriptWritable` - 脚本可写
- `BlueprintReadOnly` - Blueprint只读
- `BlueprintReadWrite` - Blueprint读写
- `SaveGame` - 可保存
- `Config` - 可配置
- `EditAnywhere` - 编辑器可编辑
- `VisibleAnywhere` - 编辑器可见

### HFUNCTION标记
- `ScriptCallable` - 脚本可调用
- `ScriptEvent` - 脚本事件
- `BlueprintCallable` - Blueprint可调用
- `BlueprintEvent` - Blueprint事件
- `BlueprintPure` - Blueprint纯函数
- `NetMulticast` - 网络多播
- `NetServer` - 服务器端
- `NetClient` - 客户端

## 🔧 集成到构建系统

在BUILD.bazel中添加：
```bazel
genrule(
    name = "generate_reflection",
    srcs = glob(["*.h"]),
    outs = ["Player.GEN.h"],
    cmd = "$(location //Shared/Reflection:reflection_codegen) $< $@",
    tools = ["//Shared/Reflection:reflection_codegen"],
)
```