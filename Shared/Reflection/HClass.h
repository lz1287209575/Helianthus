#pragma once

// 核心宏定义 - 完全匹配您的需求
#define HCLASS(...) \
    __attribute__((annotate("hclass:" #__VA_ARGS__)))

#define HPROPERTY(...) \
    __attribute__((annotate("hproperty:" #__VA_ARGS__)))

#define HFUNCTION(...) \
    __attribute__((annotate("hfunction:" #__VA_ARGS__)))

// 属性标记
#define ScriptReadable \
    "ScriptReadable"
#define ScriptWritable \
    "ScriptWritable"
#define ScriptCallable \
    "ScriptCallable"
#define ScriptEvent \
    "ScriptEvent"
#define BlueprintReadOnly \
    "BlueprintReadOnly"
#define BlueprintReadWrite \
    "BlueprintReadWrite"
#define SaveGame \
    "SaveGame"
#define Config \
    "Config"
#define EditAnywhere \
    "EditAnywhere"
#define VisibleAnywhere \
    "VisibleAnywhere"

// 使用示例将自动生成：
//
// Player.h:
// #include "Player.GEN.h"
//
// HCLASS(Scriptable)
// class Player : public HObject
// {
// public:
//     HPROPERTY(ScriptReadable)
//     int Level;
//
//     HPROPERTY(ScriptReadable)
//     int Exp;
//
//     HFUNCTION(ScriptCallable)
//     void OnLevelUp();
// };
//
// 自动生成 Player.GEN.h:
// #pragma once
// #include "Player.h"
//
// namespace Helianthus::Reflection
// {
//     template<>
//     struct ClassInfo<Player>
//     {
//         static constexpr const char* Name = "Player";
//         static constexpr PropertyInfo Properties[] = {
//             { "Level", "int", offsetof(Player, Level), ScriptReadable },
//             { "Exp", "int", offsetof(Player, Exp), ScriptReadable }
//         };
//         static constexpr FunctionInfo Functions[] = {
//             { "OnLevelUp", "void", &Player::OnLevelUp, ScriptCallable }
//         };
//     };
// }