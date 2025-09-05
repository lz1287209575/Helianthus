# Network 模块 CMakeLists.txt

# 源文件
set(NETWORK_SOURCES
    NetworkManager.cpp
    Sockets/TcpSocket.cpp
    Sockets/UdpSocket.cpp
    asio/AsyncTcpSocket.cpp
    asio/AsyncUdpSocket.cpp
    asio/AsyncTcpAcceptor.cpp
    asio/IoContext.cpp
    asio/ReactorEpoll.cpp
    asio/ReactorKqueue.cpp
    asio/ReactorIocp.cpp
    asio/ProactorIocp.cpp
    asio/ProactorReactorAdapter.cpp
    asio/MessageProtocol.cpp
    asio/ErrorMapping.cpp
    asio/PerformanceMetrics.cpp
    asio/ZeroCopyBuffer.cpp
    asio/BufferPool.cpp
    asio/MemoryMappedFile.cpp
    Security/ITlsChannel.cpp
    Security/MockTlsChannel.cpp
    WinSockInit.cpp
)

# 头文件
set(NETWORK_HEADERS
    NetworkManager.h
    TcpSocket.h
    UdpSocket.h
    WebSocketServer.h
    ConnectionPool.h
    NetworkMetrics.h
    NetworkTypes.h
)

# 创建库
add_library(helianthus_network STATIC
    ${NETWORK_SOURCES}
    ${NETWORK_HEADERS}
)

# 设置包含目录
target_include_directories(helianthus_network PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/helianthus/Network>
)

# 链接依赖
target_link_libraries(helianthus_network PUBLIC
    helianthus_common
)

# 设置编译特性
target_compile_features(helianthus_network PUBLIC cxx_std_20)

# 设置系统库
if(UNIX AND NOT APPLE)
    target_link_libraries(helianthus_network PUBLIC pthread)
elseif(WIN32)
    target_link_libraries(helianthus_network PUBLIC ws2_32)
endif()

# 设置输出名称
set_target_properties(helianthus_network PROPERTIES
    OUTPUT_NAME "helianthus_network"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
