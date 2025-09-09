# RPC 模块 CMakeLists.txt

# 源文件
set(RPC_SOURCES
    RpcServer.cpp
    RpcClient.cpp
    RpcMessage.cpp
    RpcSerializer.cpp
    RpcMessageHandler.cpp
    IRpcInterceptor.cpp
    RpcInterceptors.cpp
    MessageRouter.cpp
    NetworkAdapter.cpp
    NetworkAdapterFixed.cpp
    NetworkAdapterSimple.cpp
)

# 头文件
set(RPC_HEADERS
    IRpcServer.h
    IRpcClient.h
    RpcMessage.h
    RpcSerializer.h
    RpcMessageHandler.h
    IRpcInterceptor.h
    RpcInterceptors.h
    MessageRouter.h
    NetworkAdapter.h
    NetworkAdapterFixed.h
    NetworkAdapterSimple.h
    RpcTypes.h
)

# 创建库
add_library(helianthus_rpc STATIC
    ${RPC_SOURCES}
    ${RPC_HEADERS}
)

# 设置包含目录
target_include_directories(helianthus_rpc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/helianthus/RPC>
)

# 链接依赖
target_link_libraries(helianthus_rpc PUBLIC
    helianthus_common
    helianthus_network
    helianthus_message
    nlohmann_json::nlohmann_json
)

# 设置编译特性
target_compile_features(helianthus_rpc PUBLIC cxx_std_20)

# 设置输出名称
set_target_properties(helianthus_rpc PROPERTIES
    OUTPUT_NAME "helianthus_rpc"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)


