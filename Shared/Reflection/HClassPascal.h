#pragma once

#include "MetaSystem.h"
#include "HObject.h"

// PascalCase命名规范的反射宏
#define HCLASS(...) \
    __attribute__((annotate("HClass:" #__VA_ARGS__)))

#define HPROPERTY(...) \
    __attribute__((annotate("HProperty:" #__VA_ARGS__)))

#define HFUNCTION(...) \
    __attribute__((annotate("HFunction:" #__VA_ARGS__)))

// PascalCase属性标记
#define ScriptReadable \
    "ScriptReadable"
#define ScriptWritable \
    "ScriptWritable"
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
#define EditDefaultsOnly \
    "EditDefaultsOnly"
#define VisibleAnywhere \
    "VisibleAnywhere"
#define VisibleDefaultsOnly \
    "VisibleDefaultsOnly"
#define Category \
    "Category"
#define DisplayName \
    "DisplayName"
#define Tooltip \
    "Tooltip"

// PascalCase函数标记
#define ScriptCallable \
    "ScriptCallable"
#define ScriptEvent \
    "ScriptEvent"
#define BlueprintCallable \
    "BlueprintCallable"
#define BlueprintEvent \
    "BlueprintEvent"
#define BlueprintPure \
    "BlueprintPure"
#define NetMulticast \
    "NetMulticast"
#define NetServer \
    "NetServer"
#define NetClient \
    "NetClient"
#define AuthorityOnly \
    "AuthorityOnly"

// PascalCase类标记
#define Scriptable \
    "Scriptable"
#define BlueprintType \
    "BlueprintType"
#define ConfigClass \
    "ConfigClass"
#define DefaultConfig \
    "DefaultConfig"

// 使用示例（PascalCase命名）
/*
#include "Player.GEN.h"

HCLASS(Scriptable)
class Player : public HObject
{
public:
    HPROPERTY(ScriptReadable | Category="Stats")
    int Level = 1;

    HPROPERTY(ScriptReadable | BlueprintReadWrite | Category="Stats")
    int Experience = 0;

    HPROPERTY(SaveGame | Config | Category="Economy")
    int Gold = 100;

    HFUNCTION(ScriptCallable | Category="Leveling")
    void OnLevelUp();

    HFUNCTION(BlueprintCallable | Category="Economy")
    void AddGold(int Amount);
};
*/

// 运行时访问
namespace Helianthus::Reflection
{
    class ReflectionAPI
    {
    public:
        static Meta::ReflectionRegistry& GetRegistry();
        
        template<typename ClassType>
        static const Meta::ReflectedClass* GetClassInfo();
        
        template<typename ClassType>
        static const Meta::ReflectedProperty* GetPropertyInfo(const std::string& PropertyName);
        
        template<typename ClassType>
        static const Meta::ReflectedFunction* GetFunctionInfo(const std::string& FunctionName);
        
        static std::vector<std::string> GetClassNames();
        static std::vector<std::string> GetPropertyNames(const std::string& ClassName);
        static std::vector<std::string> GetFunctionNames(const std::string& ClassName);
    };
}