# Scripting 模块 CMakeLists.txt

# 源文件
set(SCRIPTING_SOURCES
    HotReloadManager.cpp
    LuaScriptEngine.cpp
    PythonScriptEngine.cpp
)

# 头文件
set(SCRIPTING_HEADERS
    HotReloadManager.h
    IScriptEngine.h
    LuaScriptEngine.h
    PythonScriptEngine.h
    ScriptingTypes.h
)

# 创建库
add_library(helianthus_scripting STATIC
    ${SCRIPTING_SOURCES}
    ${SCRIPTING_HEADERS}
)

# 设置包含目录
target_include_directories(helianthus_scripting PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/helianthus/Scripting>
)

# 链接依赖
target_link_libraries(helianthus_scripting PUBLIC
    helianthus_common
    helianthus_reflection
)

# 设置编译特性
target_compile_features(helianthus_scripting PUBLIC cxx_std_20)

# 条件编译脚本引擎
if(HELIANTHUS_ENABLE_LUA)
    target_compile_definitions(helianthus_scripting PUBLIC HELIANTHUS_ENABLE_LUA=1)
    # 这里可以添加 Lua 库的链接
endif()

if(HELIANTHUS_ENABLE_PYTHON)
    target_compile_definitions(helianthus_scripting PUBLIC HELIANTHUS_ENABLE_PYTHON=1)
    # 这里可以添加 Python 库的链接
endif()

if(HELIANTHUS_ENABLE_JS)
    target_compile_definitions(helianthus_scripting PUBLIC HELIANTHUS_ENABLE_JS=1)
    # 这里可以添加 V8 库的链接
endif()

if(HELIANTHUS_ENABLE_DOTNET)
    target_compile_definitions(helianthus_scripting PUBLIC HELIANTHUS_ENABLE_DOTNET=1)
    # 这里可以添加 .NET 库的链接
endif()

# 设置输出名称
set_target_properties(helianthus_scripting PROPERTIES
    OUTPUT_NAME "helianthus_scripting"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)


