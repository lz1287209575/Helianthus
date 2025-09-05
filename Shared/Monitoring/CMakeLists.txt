# Monitoring 模块 CMakeLists.txt

# 源文件
set(MONITORING_SOURCES
    PrometheusExporter.cpp
    MetricsCollector.cpp
    PerformanceMonitor.cpp
)

# 头文件
set(MONITORING_HEADERS
    PrometheusExporter.h
    MetricsCollector.h
    PerformanceMonitor.h
    MonitoringTypes.h
)

# 创建库
add_library(helianthus_monitoring STATIC
    ${MONITORING_SOURCES}
    ${MONITORING_HEADERS}
)

# 设置包含目录
target_include_directories(helianthus_monitoring PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/helianthus/Monitoring>
)

# 链接依赖
target_link_libraries(helianthus_monitoring PUBLIC
    helianthus_common
    helianthus_message_queue
)

# 设置编译特性
target_compile_features(helianthus_monitoring PUBLIC cxx_std_20)

# 设置输出名称
set_target_properties(helianthus_monitoring PROPERTIES
    OUTPUT_NAME "helianthus_monitoring"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)


