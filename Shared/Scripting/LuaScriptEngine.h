#pragma once

#include "IScriptEngine.h"
#include <unordered_set>

#ifdef ENABLE_LUA_SCRIPTING
extern "C" {
#include "ThirdParty/lua/lua.h"
#include "ThirdParty/lua/lualib.h"
#include "ThirdParty/lua/lauxlib.h"
}
#endif

namespace Helianthus::Scripting
{
    class LuaScriptEngine final : public IScriptEngine
    {
    public:
        ~LuaScriptEngine() override;

        ScriptLanguage GetLanguage() const override;
        ScriptResult Initialize() override;
        void Shutdown() override;
        ScriptResult LoadFile(const std::string& Path) override;
        ScriptResult ExecuteString(const std::string& Code) override;
        ScriptResult CallFunction(const std::string& Name, const std::vector<std::string>& Args) override;
        ScriptResult ReloadFile(const std::string& Path) override;
        void SetHotReloadCallback(HotReloadCallback Callback) override;
        bool IsFileLoaded(const std::string& Path) const override;
        std::vector<std::string> GetLoadedFiles() const override;

    private:
#ifdef ENABLE_LUA_SCRIPTING
        lua_State* LuaState = nullptr;
#else
        void* LuaState = nullptr;
#endif

        // Track loaded files and hot-reload callback irrespective of backend availability
        HotReloadCallback HotReloadHandler{};
        std::unordered_set<std::string> LoadedFileSet{};
    };
}


