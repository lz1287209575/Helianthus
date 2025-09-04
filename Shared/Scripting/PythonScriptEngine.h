#pragma once

#include <memory>
#include <string>
#include <unordered_set>

#include "IScriptEngine.h"

#ifdef ENABLE_PYTHON_SCRIPTING
    // Python C API 头文件
    #include <Python.h>
#endif

namespace Helianthus::Scripting
{
class PythonScriptEngine final : public IScriptEngine
{
public:
    PythonScriptEngine();
    ~PythonScriptEngine() override;

    ScriptLanguage GetLanguage() const override;
    ScriptResult Initialize() override;
    void Shutdown() override;
    ScriptResult LoadFile(const std::string& Path) override;
    ScriptResult ExecuteString(const std::string& Code) override;
    ScriptResult CallFunction(const std::string& Name,
                              const std::vector<std::string>& Args) override;
    ScriptResult ReloadFile(const std::string& Path) override;
    void SetHotReloadCallback(HotReloadCallback Callback) override;
    bool IsFileLoaded(const std::string& Path) const override;
    std::vector<std::string> GetLoadedFiles() const override;

private:
#ifdef ENABLE_PYTHON_SCRIPTING
    // Python相关成员
    PyObject* MainModule = nullptr;
    PyObject* GlobalDict = nullptr;
#else
    void* MainModule = nullptr;
    void* GlobalDict = nullptr;
#endif

    // 通用成员
    HotReloadCallback HotReloadHandler{};
    std::unordered_set<std::string> LoadedFileSet{};

    // Python辅助函数
    bool InitializePython();
    void ShutdownPython();
    std::string GetPythonError();
};
}  // namespace Helianthus::Scripting
