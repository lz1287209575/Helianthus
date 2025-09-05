# Discovery 模块 CMakeLists.txt

# 源文件
set(DISCOVERY_SOURCES
    ServiceRegistry.cpp
    ServiceDiscovery.cpp
    HealthChecker.cpp
    LoadBalancer.cpp
)

# 头文件
set(DISCOVERY_HEADERS
    ServiceRegistry.h
    ServiceDiscovery.h
    HealthChecker.h
    LoadBalancer.h
    DiscoveryTypes.h
)

# 创建库
add_library(helianthus_discovery STATIC
    ${DISCOVERY_SOURCES}
    ${DISCOVERY_HEADERS}
)

# 设置包含目录
target_include_directories(helianthus_discovery PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/helianthus/Discovery>
)

# 链接依赖
target_link_libraries(helianthus_discovery PUBLIC
    helianthus_common
    helianthus_network
    helianthus_config
)

# 设置编译特性
target_compile_features(helianthus_discovery PUBLIC cxx_std_20)

# 设置输出名称
set_target_properties(helianthus_discovery PROPERTIES
    OUTPUT_NAME "helianthus_discovery"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)


