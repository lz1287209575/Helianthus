# Shared 组件 CMakeLists.txt

# 添加子目录
add_subdirectory(Common)
add_subdirectory(Message)
add_subdirectory(MessageQueue)
add_subdirectory(Config)
add_subdirectory(Reflection)
add_subdirectory(ThirdParty)
add_subdirectory(Database)

# 创建主库目标
add_library(helianthus_shared INTERFACE)

# 设置包含目录
target_include_directories(helianthus_shared INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/Shared>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/helianthus>
)

# 链接所有子库
target_link_libraries(helianthus_shared INTERFACE
    helianthus_common
    helianthus_message
    helianthus_message_queue
    helianthus_config
    helianthus_reflection
    helianthus_third_party
)

# 设置编译特性
target_compile_features(helianthus_shared INTERFACE cxx_std_20)

# 设置预处理器定义
target_compile_definitions(helianthus_shared INTERFACE
    HELIANTHUS_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    HELIANTHUS_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    HELIANTHUS_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# 设置链接库
target_link_libraries(helianthus_shared INTERFACE ${HELIANTHUS_LIBS})
