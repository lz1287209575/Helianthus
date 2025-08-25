#include "ScriptBinding.h"
#include "ReflectionTypes.h"
#include <fstream>
#include <sstream>

namespace Helianthus::Reflection
{
    // 全局脚本绑定管理器
    std::unique_ptr<ScriptBindingManager> GlobalScriptBindingManager;

    // Lua脚本绑定实现
    bool LuaScriptBinding::BindClass(const std::string& ClassName, std::shared_ptr<Scripting::IScriptEngine> Engine)
    {
        if (!GlobalReflectionSystem || !Engine)
        {
            return false;
        }

        const ClassInfo* Info = GlobalReflectionSystem->GetClassInfo(ClassName);
        if (!Info)
        {
            return false;
        }

        // 生成Lua绑定代码
        std::string BindingCode = GenerateLuaClassBinding(*Info);
        
        // 执行绑定代码
        auto Result = Engine->ExecuteString(BindingCode);
        return Result.Success;
    }

    bool LuaScriptBinding::BindEnum(const std::string& EnumName, std::shared_ptr<Scripting::IScriptEngine> Engine)
    {
        if (!GlobalReflectionSystem || !Engine)
        {
            return false;
        }

        const EnumInfo* Info = GlobalReflectionSystem->GetEnumInfo(EnumName);
        if (!Info)
        {
            return false;
        }

        // 生成Lua绑定代码
        std::string BindingCode = GenerateLuaEnumBinding(*Info);
        
        // 执行绑定代码
        auto Result = Engine->ExecuteString(BindingCode);
        return Result.Success;
    }

    bool LuaScriptBinding::BindAllTypes(std::shared_ptr<Scripting::IScriptEngine> Engine)
    {
        if (!GlobalReflectionSystem || !Engine)
        {
            return false;
        }

        bool Success = true;

        // 绑定所有枚举
        auto EnumNames = GlobalReflectionSystem->GetAllEnumNames();
        for (const auto& EnumName : EnumNames)
        {
            if (!BindEnum(EnumName, Engine))
            {
                Success = false;
            }
        }

        // 绑定所有类
        auto ClassNames = GlobalReflectionSystem->GetAllClassNames();
        for (const auto& ClassName : ClassNames)
        {
            if (!BindClass(ClassName, Engine))
            {
                Success = false;
            }
        }

        return Success;
    }

    void* LuaScriptBinding::CreateScriptObject(const std::string& ClassName, std::shared_ptr<Scripting::IScriptEngine> Engine)
    {
        // 简化实现，实际需要更复杂的对象创建逻辑
        return GlobalReflectionSystem->CreateObject(ClassName);
    }

    void* LuaScriptBinding::CallScriptMethod(void* Object, const std::string& MethodName, 
                                           const std::vector<void*>& Arguments, 
                                           std::shared_ptr<Scripting::IScriptEngine> Engine)
    {
        // 简化实现，实际需要更复杂的方法调用逻辑
        return GlobalReflectionSystem->CallMethod(Object, MethodName, Arguments);
    }

    void* LuaScriptBinding::GetScriptProperty(void* Object, const std::string& PropertyName, 
                                            std::shared_ptr<Scripting::IScriptEngine> Engine)
    {
        // 简化实现，实际需要更复杂的属性访问逻辑
        return GlobalReflectionSystem->GetProperty(Object, PropertyName);
    }

    void LuaScriptBinding::SetScriptProperty(void* Object, const std::string& PropertyName, void* Value, 
                                           std::shared_ptr<Scripting::IScriptEngine> Engine)
    {
        // 简化实现，实际需要更复杂的属性设置逻辑
        GlobalReflectionSystem->SetProperty(Object, PropertyName, Value);
    }

    std::string LuaScriptBinding::GenerateLuaClassBinding(const ClassInfo& ClassInfo)
    {
        std::ostringstream Code;
        
        Code << "-- Generated Lua binding for class: " << ClassInfo.Name << "\n";
        Code << "local " << ClassInfo.Name << " = {}\n";
        Code << ClassInfo.Name << ".__index = " << ClassInfo.Name << "\n\n";
        
        // 生成构造函数
        Code << GenerateLuaConstructor(ClassInfo);
        
        // 生成属性访问器
        Code << GenerateLuaPropertyAccessors(ClassInfo);
        
        // 生成方法绑定
        Code << GenerateLuaMethodBindings(ClassInfo);
        
        // 生成元表
        Code << GenerateLuaMetatable(ClassInfo);
        
        // 导出到全局
        Code << "_G." << ClassInfo.Name << " = " << ClassInfo.Name << "\n";
        Code << "return " << ClassInfo.Name << "\n";
        
        return Code.str();
    }

    std::string LuaScriptBinding::GenerateLuaEnumBinding(const EnumInfo& EnumInfo)
    {
        std::ostringstream Code;
        
        Code << "-- Generated Lua binding for enum: " << EnumInfo.Name << "\n";
        Code << "local " << EnumInfo.Name << " = {\n";
        
        for (const auto& Value : EnumInfo.Values)
        {
            Code << "    " << Value.Name << " = " << Value.Value << ",\n";
        }
        
        Code << "}\n";
        Code << "_G." << EnumInfo.Name << " = " << EnumInfo.Name << "\n";
        Code << "return " << EnumInfo.Name << "\n";
        
        return Code.str();
    }

    std::string LuaScriptBinding::GenerateLuaConstructor(const ClassInfo& ClassInfo)
    {
        std::ostringstream Code;
        
        Code << "function " << ClassInfo.Name << ".new(...)\n";
        Code << "    local self = setmetatable({}, " << ClassInfo.Name << ")\n";
        Code << "    -- TODO: Initialize properties\n";
        Code << "    return self\n";
        Code << "end\n\n";
        
        return Code.str();
    }

    std::string LuaScriptBinding::GenerateLuaPropertyAccessors(const ClassInfo& ClassInfo)
    {
        std::ostringstream Code;
        
        for (const auto& Property : ClassInfo.Properties)
        {
            // Getter
            Code << "function " << ClassInfo.Name << ":get" << Property.Name << "()\n";
            Code << "    return self." << Property.Name << "\n";
            Code << "end\n";
            
            // Setter (如果不是只读)
            if (!Property.IsReadOnly)
            {
                Code << "function " << ClassInfo.Name << ":set" << Property.Name << "(value)\n";
                Code << "    self." << Property.Name << " = value\n";
                Code << "end\n";
            }
            
            Code << "\n";
        }
        
        return Code.str();
    }

    std::string LuaScriptBinding::GenerateLuaMethodBindings(const ClassInfo& ClassInfo)
    {
        std::ostringstream Code;
        
        for (const auto& Method : ClassInfo.Methods)
        {
            Code << "function " << ClassInfo.Name << ":" << Method.Name << "(";
            
            // 参数列表
            for (size_t i = 0; i < Method.Parameters.size(); ++i)
            {
                if (i > 0) Code << ", ";
                Code << Method.Parameters[i].Name;
            }
            
            Code << ")\n";
            Code << "    -- TODO: Implement method call\n";
            Code << "    return nil\n";
            Code << "end\n\n";
        }
        
        return Code.str();
    }

    std::string LuaScriptBinding::GenerateLuaMetatable(const ClassInfo& ClassInfo)
    {
        std::ostringstream Code;
        
        Code << "-- Metatable for " << ClassInfo.Name << "\n";
        Code << "local mt = {\n";
        Code << "    __index = " << ClassInfo.Name << ",\n";
        Code << "    __tostring = function(self)\n";
        Code << "        return \"" << ClassInfo.Name << " instance\"\n";
        Code << "    end\n";
        Code << "}\n";
        Code << "setmetatable(" << ClassInfo.Name << ", mt)\n\n";
        
        return Code.str();
    }

    // 脚本绑定管理器实现
    ScriptBindingManager::ScriptBindingManager()
    {
        LuaBinding = std::make_unique<LuaScriptBinding>();
    }

    ScriptBindingManager::~ScriptBindingManager() = default;

    void ScriptBindingManager::SetScriptEngine(std::shared_ptr<Scripting::IScriptEngine> Engine)
    {
        ScriptEngine = Engine;
    }

    std::shared_ptr<Scripting::IScriptEngine> ScriptBindingManager::GetScriptEngine() const
    {
        return ScriptEngine;
    }

    bool ScriptBindingManager::BindReflectionToScript()
    {
        if (!ScriptEngine || !LuaBinding)
        {
            return false;
        }

        return LuaBinding->BindAllTypes(ScriptEngine);
    }

    void* ScriptBindingManager::CreateScriptObject(const std::string& ClassName)
    {
        if (!ScriptEngine || !LuaBinding)
        {
            return nullptr;
        }

        return LuaBinding->CreateScriptObject(ClassName, ScriptEngine);
    }

    void* ScriptBindingManager::CallScriptMethod(void* Object, const std::string& MethodName, 
                                               const std::vector<void*>& Arguments)
    {
        if (!ScriptEngine || !LuaBinding)
        {
            return nullptr;
        }

        return LuaBinding->CallScriptMethod(Object, MethodName, Arguments, ScriptEngine);
    }

    void* ScriptBindingManager::GetScriptProperty(void* Object, const std::string& PropertyName)
    {
        if (!ScriptEngine || !LuaBinding)
        {
            return nullptr;
        }

        return LuaBinding->GetScriptProperty(Object, PropertyName, ScriptEngine);
    }

    void ScriptBindingManager::SetScriptProperty(void* Object, const std::string& PropertyName, void* Value)
    {
        if (!ScriptEngine || !LuaBinding)
        {
            return;
        }

        LuaBinding->SetScriptProperty(Object, PropertyName, Value, ScriptEngine);
    }

    std::string ScriptBindingManager::GenerateBindingCode(const std::string& Language)
    {
        if (Language == "lua" && LuaBinding)
        {
            // 生成所有类型的绑定代码
            std::ostringstream Code;
            
            if (GlobalReflectionSystem)
            {
                // 生成枚举绑定
                auto EnumNames = GlobalReflectionSystem->GetAllEnumNames();
                for (const auto& EnumName : EnumNames)
                {
                    const EnumInfo* Info = GlobalReflectionSystem->GetEnumInfo(EnumName);
                    if (Info)
                    {
                        Code << LuaBinding->GenerateLuaEnumBinding(*Info) << "\n\n";
                    }
                }
                
                // 生成类绑定
                auto ClassNames = GlobalReflectionSystem->GetAllClassNames();
                for (const auto& ClassName : ClassNames)
                {
                    const ClassInfo* Info = GlobalReflectionSystem->GetClassInfo(ClassName);
                    if (Info)
                    {
                        Code << LuaBinding->GenerateLuaClassBinding(*Info) << "\n\n";
                    }
                }
            }
            
            return Code.str();
        }
        
        return "";
    }

    bool ScriptBindingManager::SaveBindingCode(const std::string& FilePath, const std::string& Language)
    {
        std::string Code = GenerateBindingCode(Language);
        if (Code.empty())
        {
            return false;
        }

        std::ofstream File(FilePath);
        if (!File.is_open())
        {
            return false;
        }

        File << Code;
        return true;
    }

    // 脚本绑定系统初始化
    void InitializeScriptBinding()
    {
        if (!GlobalScriptBindingManager)
        {
            GlobalScriptBindingManager = std::make_unique<ScriptBindingManager>();
        }
    }

    void ShutdownScriptBinding()
    {
        GlobalScriptBindingManager.reset();
    }

} // namespace Helianthus::Reflection
