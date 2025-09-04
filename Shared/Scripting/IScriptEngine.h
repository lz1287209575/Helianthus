#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Helianthus::Scripting
{
enum class ScriptLanguage
{
    Lua,
    Python,
    JavaScript,
    CSharp,
};

struct ScriptResult
{
    bool Success = false;
    std::string ErrorMessage;
};

// 热更新回调函数类型
using HotReloadCallback = std::function<void(
    const std::string& ScriptPath, bool Success, const std::string& ErrorMessage)>;

class IScriptEngine
{
public:
    virtual ~IScriptEngine() = default;

    virtual ScriptLanguage GetLanguage() const = 0;

    virtual ScriptResult Initialize() = 0;
    virtual void Shutdown() = 0;

    virtual ScriptResult LoadFile(const std::string& Path) = 0;
    virtual ScriptResult ExecuteString(const std::string& Code) = 0;

    // 调用脚本中的函数（可扩展为多值返回、反射绑定）
    virtual ScriptResult CallFunction(const std::string& Name,
                                      const std::vector<std::string>& Args) = 0;

    // 热更新相关接口
    virtual ScriptResult ReloadFile(const std::string& Path) = 0;
    virtual void SetHotReloadCallback(HotReloadCallback Callback) = 0;
    virtual bool IsFileLoaded(const std::string& Path) const = 0;
    virtual std::vector<std::string> GetLoadedFiles() const = 0;
};
}  // namespace Helianthus::Scripting
