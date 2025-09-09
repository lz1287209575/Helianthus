# MessageQueue 模块 CMakeLists.txt

# 源文件
set(MESSAGE_QUEUE_SOURCES
    MessageQueue.cpp
    MessagePersistence.cpp
    HealthCheck.cpp
    PerformanceOptimizer.cpp
    MessageQueueCompression.cpp
    MessageQueueAlerts.cpp
    MessageQueueBatching.cpp
    MessageQueuePerf.cpp
    MessageQueuePersistence.cpp
)

# 头文件
set(MESSAGE_QUEUE_HEADERS
    IMessageQueue.h
    MessageQueue.h
    MessagePersistence.h
    HealthCheck.h
    MessageTypes.h
    PerformanceOptimizer.h
)

# 创建库
add_library(helianthus_message_queue STATIC
    ${MESSAGE_QUEUE_SOURCES}
    ${MESSAGE_QUEUE_HEADERS}
)

# 设置包含目录
target_include_directories(helianthus_message_queue PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/helianthus/MessageQueue>
)

# 链接依赖
target_link_libraries(helianthus_message_queue PUBLIC
    helianthus_common
    helianthus_message
    helianthus_network
    helianthus_discovery
    helianthus_database
    helianthus_config
    helianthus_scripting
    helianthus_rpc
    ZLIB::ZLIB
    helianthus_third_party
)

# 设置编译特性
target_compile_features(helianthus_message_queue PUBLIC cxx_std_20)

# 设置链接选项
if(UNIX AND NOT APPLE)
    target_link_libraries(helianthus_message_queue PUBLIC ssl crypto)
elseif(WIN32)
    target_link_libraries(helianthus_message_queue PUBLIC ws2_32 crypt32)
endif()

# 设置输出名称
set_target_properties(helianthus_message_queue PROPERTIES
    OUTPUT_NAME "helianthus_message_queue"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)


