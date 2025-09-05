# Message 模块 CMakeLists.txt

# 源文件
set(MESSAGE_SOURCES
    Message.cpp
)

# 头文件
set(MESSAGE_HEADERS
    Message.h
    MessageTypes.h
)

# 创建库
add_library(helianthus_message STATIC
    ${MESSAGE_SOURCES}
    ${MESSAGE_HEADERS}
)

# 设置包含目录
target_include_directories(helianthus_message PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/helianthus/Message>
)

# 链接依赖
target_link_libraries(helianthus_message PUBLIC
    helianthus_common
)

# 设置编译特性
target_compile_features(helianthus_message PUBLIC cxx_std_20)

# 设置输出名称
set_target_properties(helianthus_message PROPERTIES
    OUTPUT_NAME "helianthus_message"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)


