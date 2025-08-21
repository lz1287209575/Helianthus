#include "LuaScriptEngine.h"

namespace Helianthus::Scripting
{
    LuaScriptEngine::~LuaScriptEngine()
    {
        Shutdown();
    }

    ScriptLanguage LuaScriptEngine::GetLanguage() const
    {
        return ScriptLanguage::Lua;
    }

    ScriptResult LuaScriptEngine::Initialize()
    {
#ifdef ENABLE_LUA_SCRIPTING
        LuaState = luaL_newstate();
        if (!LuaState)
        {
            return {false, "Failed to create Lua state"};
        }
        
        luaL_openlibs(LuaState);
        return {true, {}};
#else
        return {true, {}};
#endif
    }

    void LuaScriptEngine::Shutdown()
    {
#ifdef ENABLE_LUA_SCRIPTING
        if (LuaState)
        {
            lua_close(LuaState);
            LuaState = nullptr;
        }
#endif
    }

    ScriptResult LuaScriptEngine::LoadFile(const std::string& Path)
    {
#ifdef ENABLE_LUA_SCRIPTING
        if (!LuaState)
        {
            return {false, "Lua state not initialized"};
        }

        int Result = luaL_loadfile(LuaState, Path.c_str());
        if (Result != LUA_OK)
        {
            const char* ErrorMsg = lua_tostring(LuaState, -1);
            lua_pop(LuaState, 1);
            return {false, ErrorMsg ? ErrorMsg : "Unknown error loading file"};
        }

        Result = lua_pcall(LuaState, 0, 0, 0);
        if (Result != LUA_OK)
        {
            const char* ErrorMsg = lua_tostring(LuaState, -1);
            lua_pop(LuaState, 1);
            return {false, ErrorMsg ? ErrorMsg : "Unknown error executing file"};
        }

        return {true, {}};
#else
        (void)Path;
        return {true, {}};
#endif
    }

    ScriptResult LuaScriptEngine::ExecuteString(const std::string& Code)
    {
#ifdef ENABLE_LUA_SCRIPTING
        if (!LuaState)
        {
            return {false, "Lua state not initialized"};
        }

        int Result = luaL_loadstring(LuaState, Code.c_str());
        if (Result != LUA_OK)
        {
            const char* ErrorMsg = lua_tostring(LuaState, -1);
            lua_pop(LuaState, 1);
            return {false, ErrorMsg ? ErrorMsg : "Unknown error loading string"};
        }

        Result = lua_pcall(LuaState, 0, 0, 0);
        if (Result != LUA_OK)
        {
            const char* ErrorMsg = lua_tostring(LuaState, -1);
            lua_pop(LuaState, 1);
            return {false, ErrorMsg ? ErrorMsg : "Unknown error executing string"};
        }

        return {true, {}};
#else
        (void)Code;
        return {true, {}};
#endif
    }

    ScriptResult LuaScriptEngine::CallFunction(const std::string& Name, const std::vector<std::string>& Args)
    {
#ifdef ENABLE_LUA_SCRIPTING
        if (!LuaState)
        {
            return {false, "Lua state not initialized"};
        }

        // 获取全局函数
        lua_getglobal(LuaState, Name.c_str());
        if (!lua_isfunction(LuaState, -1))
        {
            lua_pop(LuaState, 1);
            return {false, "Function '" + Name + "' not found"};
        }

        // 压入参数
        for (const auto& Arg : Args)
        {
            lua_pushstring(LuaState, Arg.c_str());
        }

        // 调用函数
        int Result = lua_pcall(LuaState, static_cast<int>(Args.size()), 0, 0);
        if (Result != LUA_OK)
        {
            const char* ErrorMsg = lua_tostring(LuaState, -1);
            lua_pop(LuaState, 1);
            return {false, ErrorMsg ? ErrorMsg : "Unknown error calling function"};
        }

        return {true, {}};
#else
        (void)Name; (void)Args;
        return {true, {}};
#endif
    }
}


