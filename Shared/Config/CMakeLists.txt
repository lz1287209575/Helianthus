# Config 模块 CMakeLists.txt

# 源文件
set(CONFIG_SOURCES
    ConfigManager.cpp
    JsonConfigProvider.cpp
)

# 头文件
set(CONFIG_HEADERS
    ConfigManager.h
    IConfigProvider.h
    JsonConfigProvider.h
)

# 创建库
add_library(helianthus_config STATIC
    ${CONFIG_SOURCES}
    ${CONFIG_HEADERS}
)

# 设置包含目录
target_include_directories(helianthus_config PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/helianthus/Config>
)

# 链接依赖
target_link_libraries(helianthus_config PUBLIC
    helianthus_common
    nlohmann_json::nlohmann_json
)

# 设置编译特性
target_compile_features(helianthus_config PUBLIC cxx_std_20)

# 设置输出名称
set_target_properties(helianthus_config PROPERTIES
    OUTPUT_NAME "helianthus_config"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)


