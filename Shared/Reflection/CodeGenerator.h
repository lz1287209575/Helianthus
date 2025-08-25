#pragma once

#include "ReflectionTypes.h"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace Helianthus::Reflection
{
    // 代码生成器
    class CodeGenerator
    {
    public:
        // 生成头文件
        static std::string GenerateHeader(const std::string& ClassName, 
                                        const std::string& Namespace = "Helianthus::Reflection")
        {
            std::ostringstream oss;
            oss << "#pragma once\n\n";
            oss << "#include \"ReflectionTypes.h\"\n";
            oss << "#include <type_traits>\n\n";
            oss << "namespace " << Namespace << "\n";
            oss << "{\n";
            oss << "    // 自动生成的反射代码\n";
            oss << "    class " << ClassName << "Reflection\n";
            oss << "    {\n";
            oss << "    public:\n";
            oss << "        static void Register(ReflectionSystem* System);\n";
            oss << "        static const ClassInfo& GetClassInfo();\n";
            oss << "    };\n";
            oss << "} // namespace " << Namespace << "\n";
            return oss.str();
        }

        // 生成实现文件
        static std::string GenerateImplementation(const std::string& ClassName,
                                                const std::vector<std::string>& Properties,
                                                const std::vector<std::string>& Methods,
                                                const std::string& Namespace = "Helianthus::Reflection")
        {
            std::ostringstream oss;
            oss << "#include \"" << ClassName << "Reflection.h\"\n";
            oss << "#include \"" << ClassName << ".h\"\n\n";
            oss << "namespace " << Namespace << "\n";
            oss << "{\n";
            oss << "    void " << ClassName << "Reflection::Register(ReflectionSystem* System)\n";
            oss << "    {\n";
            oss << "        if (System)\n";
            oss << "        {\n";
            oss << "            System->RegisterClass(GetClassInfo());\n";
            oss << "        }\n";
            oss << "    }\n\n";
            oss << "    const ClassInfo& " << ClassName << "Reflection::GetClassInfo()\n";
            oss << "    {\n";
            oss << "        static ClassInfo Info;\n";
            oss << "        static bool Initialized = false;\n\n";
            oss << "        if (!Initialized)\n";
            oss << "        {\n";
            oss << "            Info.Name = \"" << ClassName << "\";\n";
            oss << "            Info.FullName = \"" << Namespace << "::" << ClassName << "\";\n";
            oss << "            Info.TypeIndex = std::type_index(typeid(" << ClassName << "));\n";
            oss << "            Info.IsAbstract = std::is_abstract_v<" << ClassName << ">;\n";
            oss << "            Info.IsFinal = std::is_final_v<" << ClassName << ">;\n\n";

            // 生成属性注册代码
            for (const auto& Property : Properties)
            {
                oss << "            // 注册属性: " << Property << "\n";
                oss << "            PropertyInfo " << Property << "Prop;\n";
                oss << "            " << Property << "Prop.Name = \"" << Property << "\";\n";
                oss << "            " << Property << "Prop.TypeName = \"auto\";\n";
                oss << "            " << Property << "Prop.Getter = [](void* Obj) -> void* {\n";
                oss << "                auto* Object = static_cast<" << ClassName << "*>(Obj);\n";
                oss << "                return static_cast<void*>(&Object->" << Property << ");\n";
                oss << "            };\n";
                oss << "            " << Property << "Prop.Setter = [](void* Obj, void* Value) {\n";
                oss << "                auto* Object = static_cast<" << ClassName << "*>(Obj);\n";
                oss << "                Object->" << Property << " = *static_cast<decltype(Object->" << Property << ")*>(Value);\n";
                oss << "            };\n";
                oss << "            Info.Properties.push_back(" << Property << "Prop);\n\n";
            }

            // 生成方法注册代码
            for (const auto& Method : Methods)
            {
                oss << "            // 注册方法: " << Method << "\n";
                oss << "            MethodInfo " << Method << "Method;\n";
                oss << "            " << Method << "Method.Name = \"" << Method << "\";\n";
                oss << "            " << Method << "Method.ReturnTypeName = \"auto\";\n";
                oss << "            " << Method << "Method.Invoker = [](void* Obj, const std::vector<void*>& Args) -> void* {\n";
                oss << "                auto* Object = static_cast<" << ClassName << "*>(Obj);\n";
                oss << "                return static_cast<void*>(&Object->" << Method << "());\n";
                oss << "            };\n";
                oss << "            Info.Methods.push_back(" << Method << "Method);\n\n";
            }

            oss << "            Initialized = true;\n";
            oss << "        }\n\n";
            oss << "        return Info;\n";
            oss << "    }\n";
            oss << "} // namespace " << Namespace << "\n";
            return oss.str();
        }

        // 生成自动注册宏
        static std::string GenerateAutoRegisterMacro(const std::string& ClassName)
        {
            std::ostringstream oss;
            oss << "#define " << ClassName << "_AUTO_REGISTER \\\n";
            oss << "    static bool Register" << ClassName << "() { \\\n";
            oss << "        if (GlobalReflectionSystem) { \\\n";
            oss << "            " << ClassName << "Reflection::Register(GlobalReflectionSystem.get()); \\\n";
            oss << "            return true; \\\n";
            oss << "        } \\\n";
            oss << "        return false; \\\n";
            oss << "    } \\\n";
            oss << "    static bool " << ClassName << "Registered = Register" << ClassName << "()\n";
            return oss.str();
        }

        // 生成CMakeLists.txt片段
        static std::string GenerateCMakeFragment(const std::string& ClassName)
        {
            std::ostringstream oss;
            oss << "# 自动生成的反射代码\n";
            oss << "set(" << ClassName << "_REFLECTION_SOURCES\n";
            oss << "    " << ClassName << "Reflection.cpp\n";
            oss << ")\n\n";
            oss << "add_library(" << ClassName << "Reflection STATIC ${" << ClassName << "_REFLECTION_SOURCES})\n";
            oss << "target_link_libraries(" << ClassName << "Reflection reflection)\n";
            return oss.str();
        }

        // 生成Bazel BUILD片段
        static std::string GenerateBazelFragment(const std::string& ClassName)
        {
            std::ostringstream oss;
            oss << "cc_library(\n";
            oss << "    name = \"" << ClassName << "_reflection\",\n";
            oss << "    srcs = [\"" << ClassName << "Reflection.cpp\"],\n";
            oss << "    hdrs = [\"" << ClassName << "Reflection.h\"],\n";
            oss << "    deps = [\"//Shared/Reflection:reflection\"],\n";
            oss << "    visibility = [\"//visibility:public\"],\n";
            oss << ")\n";
            return oss.str();
        }

        // 保存生成的文件
        static bool SaveGeneratedFile(const std::string& FilePath, const std::string& Content)
        {
            std::ofstream File(FilePath);
            if (File.is_open())
            {
                File << Content;
                File.close();
                return true;
            }
            return false;
        }

        // 生成完整的反射代码
        static bool GenerateReflectionCode(const std::string& ClassName,
                                         const std::vector<std::string>& Properties,
                                         const std::vector<std::string>& Methods,
                                         const std::string& OutputDir,
                                         const std::string& Namespace = "Helianthus::Reflection")
        {
            // 生成头文件
            std::string HeaderContent = GenerateHeader(ClassName, Namespace);
            std::string HeaderPath = OutputDir + "/" + ClassName + "Reflection.h";
            if (!SaveGeneratedFile(HeaderPath, HeaderContent))
            {
                return false;
            }

            // 生成实现文件
            std::string ImplContent = GenerateImplementation(ClassName, Properties, Methods, Namespace);
            std::string ImplPath = OutputDir + "/" + ClassName + "Reflection.cpp";
            if (!SaveGeneratedFile(ImplPath, ImplContent))
            {
                return false;
            }

            // 生成宏定义文件
            std::string MacroContent = GenerateAutoRegisterMacro(ClassName);
            std::string MacroPath = OutputDir + "/" + ClassName + "Macros.h";
            if (!SaveGeneratedFile(MacroPath, MacroContent))
            {
                return false;
            }

            return true;
        }
    };

    // 智能注册管理器
    class SmartRegistrationManager
    {
    public:
        static SmartRegistrationManager& GetInstance()
        {
            static SmartRegistrationManager Instance;
            return Instance;
        }

        // 注册类信息
        void RegisterClassInfo(const std::string& ClassName,
                              const std::vector<std::string>& Properties,
                              const std::vector<std::string>& Methods)
        {
            ClassRegistry[ClassName] = {Properties, Methods};
        }

        // 生成所有注册代码
        bool GenerateAllReflectionCode(const std::string& OutputDir)
        {
            for (const auto& [ClassName, Info] : ClassRegistry)
            {
                if (!CodeGenerator::GenerateReflectionCode(ClassName, Info.first, Info.second, OutputDir))
                {
                    return false;
                }
            }
            return true;
        }

        // 获取注册的类信息
        const std::unordered_map<std::string, std::pair<std::vector<std::string>, std::vector<std::string>>>& 
        GetClassRegistry() const
        {
            return ClassRegistry;
        }

    private:
        std::unordered_map<std::string, std::pair<std::vector<std::string>, std::vector<std::string>>> ClassRegistry;
    };

    // 智能注册宏
    #define HELIANTHUS_SMART_REGISTER_CLASS(ClassName, ...) \
        namespace Helianthus::Reflection::AutoGen { \
            static void Register##ClassName() { \
                SmartRegistrationManager::GetInstance().RegisterClassInfo( \
                    #ClassName, \
                    {__VA_ARGS__}, \
                    {} \
                ); \
            } \
            static bool ClassName##Registered = (Register##ClassName(), true); \
        }

    #define HELIANTHUS_SMART_REGISTER_METHODS(ClassName, ...) \
        namespace Helianthus::Reflection::AutoGen { \
            static void Register##ClassName##Methods() { \
                auto& Registry = SmartRegistrationManager::GetInstance(); \
                auto Info = Registry.GetClassRegistry().at(#ClassName); \
                auto Properties = Info.first; \
                auto Methods = std::vector<std::string>{__VA_ARGS__}; \
                Registry.RegisterClassInfo(#ClassName, Properties, Methods); \
            } \
            static bool ClassName##MethodsRegistered = (Register##ClassName##Methods(), true); \
        }

} // namespace Helianthus::Reflection
