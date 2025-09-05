# ThirdParty 模块 CMakeLists.txt

# 源文件
set(THIRD_PARTY_SOURCES
    tinyaes/aes.c
)

# 头文件
set(THIRD_PARTY_HEADERS
    tinyaes/aes.h
)

# 创建库
add_library(helianthus_third_party STATIC
    ${THIRD_PARTY_SOURCES}
    ${THIRD_PARTY_HEADERS}
)

# 设置包含目录
target_include_directories(helianthus_third_party PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/helianthus/ThirdParty>
)

# 设置编译特性
target_compile_features(helianthus_third_party PUBLIC cxx_std_20)

# 设置输出名称
set_target_properties(helianthus_third_party PROPERTIES
    OUTPUT_NAME "helianthus_third_party"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
