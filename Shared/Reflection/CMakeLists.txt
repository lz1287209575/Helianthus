# Reflection 模块 CMakeLists.txt

# 源文件
set(REFLECTION_SOURCES
    Reflection.cpp
)

# 头文件
set(REFLECTION_HEADERS
    Reflection.h
    Macros.h
)

# 创建库
add_library(helianthus_reflection STATIC
    ${REFLECTION_SOURCES}
    ${REFLECTION_HEADERS}
)

# 设置包含目录
target_include_directories(helianthus_reflection PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/helianthus/Reflection>
)

# 链接依赖
target_link_libraries(helianthus_reflection PUBLIC
    helianthus_common
)

# 设置编译特性
target_compile_features(helianthus_reflection PUBLIC cxx_std_20)

# 设置输出名称
set_target_properties(helianthus_reflection PROPERTIES
    OUTPUT_NAME "helianthus_reflection"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)


