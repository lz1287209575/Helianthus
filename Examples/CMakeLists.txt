# Examples 目录 CMakeLists.txt

# 示例程序列表
set(EXAMPLE_PROGRAMS
    performance_benchmark
    prometheus_dump
    prometheus_exporter_example
    tcmalloc_config_example
    batch_processing_example
    wakeup_mechanism_example
    lua_hotreload_example
    game_server_with_hotreload
    performance_monitoring_example
    log_category_example
    unified_api_example
    persistence_metrics_example
    resource_monitor_example
    transaction_example
    health_check_example
    config_system_example
    rpc_example
    discovery_example
    message_queue_example
    message_queue_persistence_example
    simple_persistence_example
    logging_test_example
    multi_language_scripting_example
    dead_letter_queue_example
    simple_dead_letter_example
    dead_letter_core_test
    persistence_test
    file_persistence_test
    filesystem_test
    index_read_test
    simple_log_test
    precise_debug_test
    thread_join_test
    destructor_test
    initialize_step_test
    mutex_destructor_test
    filestream_test
    readindex_test
    simple_destructor_test
    logger_shutdown_test
    simple_send_test
    simple_receive_test
    dlq_monitor_test
    ha_failover_demo
    simple_ha_test
    transaction_test
    compression_encryption_test
    alert_test
    performance_test
    performance_optimizer_example
    enhanced_prometheus_example
    config_example
)

# 为每个示例程序创建可执行文件
foreach(example ${EXAMPLE_PROGRAMS})
    # 检查源文件是否存在
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${example}.cpp)
        add_executable(${example}
            ${example}.cpp
        )
        
        # 设置包含目录
        target_include_directories(${example} PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/Shared
        )
        
        # 链接库
        target_link_libraries(${example} PRIVATE
            helianthus_shared
        )
        
        # 设置编译特性
        target_compile_features(${example} PRIVATE cxx_std_20)
        
        # 设置预处理器定义
        target_compile_definitions(${example} PRIVATE
            HELIANTHUS_ENABLE_LOGGING=1
        )
        
        # 设置输出目录
        set_target_properties(${example} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples
        )
    endif()
endforeach()

# 特殊处理一些需要特定依赖的示例
if(TARGET enhanced_prometheus_example)
    target_link_libraries(enhanced_prometheus_example PRIVATE
        helianthus_monitoring
    )
endif()


if(TARGET config_example)
    target_link_libraries(config_example PRIVATE
        helianthus_config
    )
endif()

if(TARGET rpc_example)
    target_link_libraries(rpc_example PRIVATE
        helianthus_rpc
    )
endif()

if(TARGET discovery_example)
    target_link_libraries(discovery_example PRIVATE
        helianthus_discovery
    )
endif()

if(TARGET lua_hotreload_example OR TARGET game_server_with_hotreload OR TARGET multi_language_scripting_example)
    target_link_libraries(lua_hotreload_example PRIVATE helianthus_scripting)
    target_link_libraries(game_server_with_hotreload PRIVATE helianthus_scripting)
    target_link_libraries(multi_language_scripting_example PRIVATE helianthus_scripting)
endif()

# 安装示例程序
install(TARGETS ${EXAMPLE_PROGRAMS}
    RUNTIME DESTINATION bin/examples
)


