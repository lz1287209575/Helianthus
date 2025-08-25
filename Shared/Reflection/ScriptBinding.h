#pragma once

#include "ReflectionTypes.h"
#include "Shared/Scripting/IScriptEngine.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Helianthus::Reflection
{
    // 脚本绑定接口
    class IScriptBinding
    {
    public:
        virtual ~IScriptBinding() = default;

        // 绑定类到脚本引擎
        virtual bool BindClass(const std::string& ClassName, std::shared_ptr<Scripting::IScriptEngine> Engine) = 0;
        
        // 绑定枚举到脚本引擎
        virtual bool BindEnum(const std::string& EnumName, std::shared_ptr<Scripting::IScriptEngine> Engine) = 0;
        
        // 绑定所有已注册的类型
        virtual bool BindAllTypes(std::shared_ptr<Scripting::IScriptEngine> Engine) = 0;
        
        // 创建脚本对象
        virtual void* CreateScriptObject(const std::string& ClassName, std::shared_ptr<Scripting::IScriptEngine> Engine) = 0;
        
        // 调用脚本方法
        virtual void* CallScriptMethod(void* Object, const std::string& MethodName, 
                                     const std::vector<void*>& Arguments, 
                                     std::shared_ptr<Scripting::IScriptEngine> Engine) = 0;
        
        // 获取脚本属性
        virtual void* GetScriptProperty(void* Object, const std::string& PropertyName, 
                                      std::shared_ptr<Scripting::IScriptEngine> Engine) = 0;
        
        // 设置脚本属性
        virtual void SetScriptProperty(void* Object, const std::string& PropertyName, void* Value, 
                                     std::shared_ptr<Scripting::IScriptEngine> Engine) = 0;
    };

    // Lua脚本绑定实现
    class LuaScriptBinding : public IScriptBinding
    {
    public:
        bool BindClass(const std::string& ClassName, std::shared_ptr<Scripting::IScriptEngine> Engine) override;
        bool BindEnum(const std::string& EnumName, std::shared_ptr<Scripting::IScriptEngine> Engine) override;
        bool BindAllTypes(std::shared_ptr<Scripting::IScriptEngine> Engine) override;
        
        void* CreateScriptObject(const std::string& ClassName, std::shared_ptr<Scripting::IScriptEngine> Engine) override;
        void* CallScriptMethod(void* Object, const std::string& MethodName, 
                             const std::vector<void*>& Arguments, 
                             std::shared_ptr<Scripting::IScriptEngine> Engine) override;
        void* GetScriptProperty(void* Object, const std::string& PropertyName, 
                              std::shared_ptr<Scripting::IScriptEngine> Engine) override;
        void SetScriptProperty(void* Object, const std::string& PropertyName, void* Value, 
                             std::shared_ptr<Scripting::IScriptEngine> Engine) override;

        // 生成Lua绑定代码
        std::string GenerateLuaClassBinding(const ClassInfo& ClassInfo);
        std::string GenerateLuaEnumBinding(const EnumInfo& EnumInfo);
        
        // 生成Lua构造函数
        std::string GenerateLuaConstructor(const ClassInfo& ClassInfo);
        
        // 生成Lua属性访问器
        std::string GenerateLuaPropertyAccessors(const ClassInfo& ClassInfo);
        
        // 生成Lua方法绑定
        std::string GenerateLuaMethodBindings(const ClassInfo& ClassInfo);
        
        // 生成Lua元表
        std::string GenerateLuaMetatable(const ClassInfo& ClassInfo);
    };

    // 脚本绑定管理器
    class ScriptBindingManager
    {
    public:
        ScriptBindingManager();
        ~ScriptBindingManager();

        // 设置脚本引擎
        void SetScriptEngine(std::shared_ptr<Scripting::IScriptEngine> Engine);
        
        // 获取脚本引擎
        std::shared_ptr<Scripting::IScriptEngine> GetScriptEngine() const;
        
        // 绑定反射类型到脚本
        bool BindReflectionToScript();
        
        // 创建脚本对象
        void* CreateScriptObject(const std::string& ClassName);
        
        // 调用脚本方法
        void* CallScriptMethod(void* Object, const std::string& MethodName, 
                             const std::vector<void*>& Arguments);
        
        // 获取脚本属性
        void* GetScriptProperty(void* Object, const std::string& PropertyName);
        
        // 设置脚本属性
        void SetScriptProperty(void* Object, const std::string& PropertyName, void* Value);
        
        // 生成脚本绑定代码
        std::string GenerateBindingCode(const std::string& Language = "lua");
        
        // 保存绑定代码到文件
        bool SaveBindingCode(const std::string& FilePath, const std::string& Language = "lua");

    private:
        std::shared_ptr<Scripting::IScriptEngine> ScriptEngine;
        std::unique_ptr<LuaScriptBinding> LuaBinding;
        std::unordered_map<std::string, bool> BoundClasses;
        std::unordered_map<std::string, bool> BoundEnums;
    };

    // 全局脚本绑定管理器
    extern std::unique_ptr<ScriptBindingManager> GlobalScriptBindingManager;

    // 脚本绑定系统初始化
    void InitializeScriptBinding();
    void ShutdownScriptBinding();

} // namespace Helianthus::Reflection
