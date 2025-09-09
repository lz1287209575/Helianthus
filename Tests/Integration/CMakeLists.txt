# Integration 测试 CMakeLists.txt

# 查找测试源文件
file(GLOB TEST_SOURCES "*.cpp")

foreach(test_file ${TEST_SOURCES})
    # 获取测试名称（去掉路径和扩展名）
    get_filename_component(test_name ${test_file} NAME_WE)
    
    # 创建测试可执行文件
    add_executable(${test_name}
        ${test_file}
    )
    
    # 设置包含目录
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/Shared
    )
    
    # 链接库
    target_link_libraries(${test_name} PRIVATE
        helianthus_common
        helianthus_message_queue
        helianthus_reflection_gen
        helianthus_rpc
        helianthus_network
        helianthus_message
        helianthus_config
        GTest::gtest
        GTest::gtest_main
    )
    
    # 设置编译特性
    target_compile_features(${test_name} PRIVATE cxx_std_20)
    
    # 设置预处理器定义
    target_compile_definitions(${test_name} PRIVATE
        HELIANTHUS_ENABLE_LOGGING=1
        HELIANTHUS_ENABLE_REDIS=1
        HELIANTHUS_ENABLE_LZ4=1
        HELIANTHUS_ENABLE_ZSTD=1
    )
    
    # 设置输出目录
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
    )
    
    # 添加到测试
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES
        TIMEOUT 300
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endforeach()
