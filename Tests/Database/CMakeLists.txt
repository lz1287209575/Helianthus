# Database 测试 CMakeLists.txt

include(CheckIncludeFile)

# 检测是否存在 mysql.h（无则降级为可编译但跳过用例）
find_path(MYSQL_INCLUDE_DIR mysql.h
    HINTS
        ${CMAKE_SOURCE_DIR}/ThirdParty/mariadb-connector-c/include
        ${CMAKE_SOURCE_DIR}/ThirdParty/mysql-connector/include
)
find_path(MARIADB_VERSION_HEADER mariadb_version.h
    HINTS
        ${CMAKE_SOURCE_DIR}/ThirdParty/mariadb-connector-c/include
        ${CMAKE_SOURCE_DIR}/ThirdParty/mysql-connector/include
)
set(HAS_MYSQL FALSE)
if(MYSQL_INCLUDE_DIR AND MARIADB_VERSION_HEADER)
    set(HAS_MYSQL TRUE)
else()
    message(STATUS "Database headers incomplete (mysql.h/mariadb_version.h), database tests will compile but be skipped at runtime")
endif()

# 查找测试源文件
file(GLOB TEST_SOURCES "*.cpp")

foreach(test_file ${TEST_SOURCES})
    # 获取测试名称（去掉路径和扩展名）
    get_filename_component(test_name ${test_file} NAME_WE)
    
    # 创建测试可执行文件
    add_executable(${test_name}
        ${test_file}
        ORM.cpp
    )
    
    # 设置包含目录
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/Shared
        ${CMAKE_SOURCE_DIR}/Shared/Database
        ${CMAKE_SOURCE_DIR}/Shared/Database/MySQL
        ${MYSQL_INCLUDE_DIR}
    )
    
    # 链接库
    target_link_libraries(${test_name} PRIVATE
        helianthus_common
        helianthus_config
        helianthus_database
        helianthus_reflection
        helianthus_third_party
        GTest::gtest
        GTest::gtest_main
    )
    
    # 设置编译特性
    target_compile_features(${test_name} PRIVATE cxx_std_20)
    
    # 设置预处理器定义
    target_compile_definitions(${test_name} PRIVATE
        HELIANTHUS_ENABLE_LOGGING=1
        HELIANTHUS_ENABLE_REDIS=1
    )

    if(NOT HAS_MYSQL)
        target_compile_definitions(${test_name} PRIVATE HELIANTHUS_DISABLE_MYSQL=1)
    endif()
    
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
