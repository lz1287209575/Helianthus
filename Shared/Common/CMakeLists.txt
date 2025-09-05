# Common 模块 CMakeLists.txt

# 源文件
set(COMMON_SOURCES
    Logger.cpp
    LogCategories.cpp
    TCMallocWrapper.cpp
    StructuredLogger.cpp
    CommandLineParser.cpp
    ResourceMonitor.cpp
)

# 头文件
set(COMMON_HEADERS
    Logger.h
    LogCategory.h
    LogCategories.h
    Types.h
    TCMallocWrapper.h
    StructuredLogger.h
    CommandLineParser.h
    ResourceMonitor.h
    Utils.h
)

# 创建库
add_library(helianthus_common STATIC
    ${COMMON_SOURCES}
    ${COMMON_HEADERS}
)

# 设置包含目录
target_include_directories(helianthus_common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/helianthus/Common>
)

# 链接依赖
target_link_libraries(helianthus_common PUBLIC
    spdlog::spdlog
)

# 设置编译特性
target_compile_features(helianthus_common PUBLIC cxx_std_20)

# 设置预处理器定义
target_compile_definitions(helianthus_common PUBLIC
    HELIANTHUS_ENABLE_LOGGING=1
)

# 设置编译选项
if(MSVC)
    target_compile_options(helianthus_common PRIVATE /W4)
else()
    target_compile_options(helianthus_common PRIVATE -Wall -Wextra)
endif()

# 设置输出名称
set_target_properties(helianthus_common PROPERTIES
    OUTPUT_NAME "helianthus_common"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)


