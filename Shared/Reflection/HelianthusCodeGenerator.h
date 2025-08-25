#pragma once

#include "HelianthusReflection.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace Helianthus::Reflection
{
    // Helianthus风格的代码生成器
    class HelianthusCodeGenerator
    {
    public:
        // 生成头文件
        static std::string GenerateHeader(const std::string& ClassName, 
                                        const std::string& SuperClassName,
                                        const std::vector<std::string>& Properties,
                                        const std::vector<std::string>& Methods,
                                        const std::string& Namespace = "Helianthus::Reflection")
        {
            std::ostringstream oss;
            oss << "#pragma once\n\n";
            oss << "#include \"HelianthusReflection.h\"\n";
            oss << "#include <string>\n\n";
            oss << "namespace " << Namespace << "\n";
            oss << "{\n";
            oss << "    // 自动生成的Helianthus风格类\n";
            oss << "    HELIANTHUS_CLASS(" << ClassName << ", " << SuperClassName << ")\n";
            oss << "    {\n";
            
            // 生成属性
            for (const auto& Property : Properties)
            {
                oss << "        HELIANTHUS_PROPERTY(" << Property << ", std::string);\n";
            }
            
            // 生成方法
            for (const auto& Method : Methods)
            {
                oss << "        HELIANTHUS_METHOD(" << Method << ", void);\n";
            }
            
            oss << "    };\n";
            oss << "} // namespace " << Namespace << "\n";
            return oss.str();
        }

        // 生成实现文件
        static std::string GenerateImplementation(const std::string& ClassName,
                                                const std::string& SuperClassName,
                                                const std::vector<std::string>& Properties,
                                                const std::vector<std::string>& Methods,
                                                const std::string& Namespace = "Helianthus::Reflection")
        {
            std::ostringstream oss;
            oss << "#include \"" << ClassName << ".h\"\n\n";
            oss << "namespace " << Namespace << "\n";
            oss << "{\n";
            
            // 生成HELIANTHUS_IMPLEMENT_CLASS
            oss << "    HELIANTHUS_IMPLEMENT_CLASS(" << ClassName << ", " << SuperClassName << ")\n\n";
            
            // 生成属性注册
            oss << "    void " << ClassName << "::RegisterProperties(HelianthusClassInfo* ClassInfo)\n";
            oss << "    {\n";
            for (const auto& Property : Properties)
            {
                oss << "        Register" << Property << "Property(ClassInfo);\n";
            }
            oss << "    }\n\n";
            
            // 生成方法注册
            oss << "    void " << ClassName << "::RegisterMethods(HelianthusClassInfo* ClassInfo)\n";
            oss << "    {\n";
            for (const auto& Method : Methods)
            {
                oss << "        Register" << Method << "Method(ClassInfo);\n";
            }
            oss << "    }\n\n";
            
            // 生成方法实现
            for (const auto& Method : Methods)
            {
                oss << "    void " << ClassName << "::" << Method << "()\n";
                oss << "    {\n";
                oss << "        // 自动生成的方法实现\n";
                oss << "    }\n\n";
            }
            
            oss << "} // namespace " << Namespace << "\n";
            return oss.str();
        }

        // 生成完整的Helianthus风格反射代码
        static bool GenerateHelianthusReflectionCode(const std::string& ClassName,
                                                   const std::string& SuperClassName,
                                                   const std::vector<std::string>& Properties,
                                                   const std::vector<std::string>& Methods,
                                                   const std::string& OutputDir,
                                                   const std::string& Namespace = "Helianthus::Reflection")
        {
            // 生成头文件
            std::string HeaderContent = GenerateHeader(ClassName, SuperClassName, Properties, Methods, Namespace);
            std::string HeaderPath = OutputDir + "/" + ClassName + ".h";
            if (!SaveGeneratedFile(HeaderPath, HeaderContent))
            {
                return false;
            }

            // 生成实现文件
            std::string ImplContent = GenerateImplementation(ClassName, SuperClassName, Properties, Methods, Namespace);
            std::string ImplPath = OutputDir + "/" + ClassName + ".cpp";
            if (!SaveGeneratedFile(ImplPath, ImplContent))
            {
                return false;
            }

            return true;
        }

        // 生成Helianthus风格的宏定义
        static std::string GenerateHelianthusMacros(const std::string& ClassName)
        {
            std::ostringstream oss;
            oss << "// Helianthus风格的宏定义\n";
            oss << "#define " << ClassName << "_HELIANTHUS_CLASS() \\\n";
            oss << "    HELIANTHUS_CLASS(" << ClassName << ", HelianthusObject)\n\n";
            oss << "#define " << ClassName << "_HELIANTHUS_PROPERTY(PropertyName, Type) \\\n";
            oss << "    HELIANTHUS_PROPERTY(PropertyName, Type)\n\n";
            oss << "#define " << ClassName << "_HELIANTHUS_METHOD(MethodName, ReturnType) \\\n";
            oss << "    HELIANTHUS_METHOD(MethodName, ReturnType)\n\n";
            oss << "#define " << ClassName << "_HELIANTHUS_IMPLEMENT() \\\n";
            oss << "    HELIANTHUS_IMPLEMENT_CLASS(" << ClassName << ", HelianthusObject)\n";
            return oss.str();
        }

        // 生成Helianthus风格的构建配置
        static std::string GenerateHelianthusBuildConfig(const std::string& ClassName)
        {
            std::ostringstream oss;
            oss << "# Helianthus风格的构建配置\n";
            oss << "cc_library(\n";
            oss << "    name = \"" << ClassName << "_helianthus_reflection\",\n";
            oss << "    srcs = [\"" << ClassName << ".cpp\"],\n";
            oss << "    hdrs = [\"" << ClassName << ".h\"],\n";
            oss << "    deps = [\n";
            oss << "        \"//Shared/Reflection:helianthus_reflection\",\n";
            oss << "    ],\n";
            oss << "    visibility = [\"//visibility:public\"],\n";
            oss << "    copts = select({\n";
            oss << "        \"@bazel_tools//src/conditions:windows\": [\"/std:c++20\", \"/utf-8\"],\n";
            oss << "        \"//conditions:default\": [\"-std=c++20\", \"-fPIC\"],\n";
            oss << "    }),\n";
            oss << ")\n";
            return oss.str();
        }

    private:
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
    };

    // Helianthus风格的反射管理器
    class HelianthusReflectionManager
    {
    public:
        static HelianthusReflectionManager& GetInstance()
        {
            static HelianthusReflectionManager Instance;
            return Instance;
        }

        // 注册Helianthus风格类
        void RegisterHelianthusClass(const std::string& ClassName,
                                   const std::string& SuperClassName,
                                   const std::vector<std::string>& Properties,
                                   const std::vector<std::string>& Methods)
        {
            HelianthusClasses[ClassName] = {SuperClassName, Properties, Methods};
        }

        // 生成所有Helianthus风格反射代码
        bool GenerateAllHelianthusReflectionCode(const std::string& OutputDir)
        {
            for (const auto& [ClassName, Info] : HelianthusClasses)
            {
                const auto& SuperClassName = std::get<0>(Info);
                const auto& Properties = std::get<1>(Info);
                const auto& Methods = std::get<2>(Info);
                
                if (!HelianthusCodeGenerator::GenerateHelianthusReflectionCode(
                    ClassName, SuperClassName, Properties, Methods, OutputDir))
                {
                    return false;
                }
            }
            return true;
        }

        // 获取注册的Helianthus类信息
        const std::unordered_map<std::string, std::tuple<std::string, std::vector<std::string>, std::vector<std::string>>>& 
        GetHelianthusClasses() const
        {
            return HelianthusClasses;
        }

    private:
        std::unordered_map<std::string, std::tuple<std::string, std::vector<std::string>, std::vector<std::string>>> HelianthusClasses;
    };

    // Helianthus风格的注册宏
    #define HELIANTHUS_REGISTER_CLASS(ClassName, SuperClassName, ...) \
        namespace Helianthus::Reflection::HelianthusGen { \
            static void Register##ClassName() { \
                HelianthusReflectionManager::GetInstance().RegisterHelianthusClass( \
                    #ClassName, \
                    #SuperClassName, \
                    {__VA_ARGS__}, \
                    {} \
                ); \
            } \
            static bool ClassName##Registered = (Register##ClassName(), true); \
        }

    #define HELIANTHUS_REGISTER_METHODS(ClassName, ...) \
        namespace Helianthus::Reflection::HelianthusGen { \
            static void Register##ClassName##Methods() { \
                auto& Manager = HelianthusReflectionManager::GetInstance(); \
                auto Info = Manager.GetHelianthusClasses().at(#ClassName); \
                auto Properties = std::get<1>(Info); \
                auto Methods = std::vector<std::string>{__VA_ARGS__}; \
                Manager.RegisterHelianthusClass(#ClassName, std::get<0>(Info), Properties, Methods); \
            } \
            static bool ClassName##MethodsRegistered = (Register##ClassName##Methods(), true); \
        }

} // namespace Helianthus::Reflection
