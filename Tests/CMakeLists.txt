# Tests 目录 CMakeLists.txt

# 查找 Google Test
find_package(GTest REQUIRED)

# 启用测试
enable_testing()

# 测试子模块开关（避免未构建导致的大量 Not Run）
option(HELIANTHUS_TEST_COMMON "Enable Common tests" ON)
option(HELIANTHUS_TEST_CONFIG "Enable Config tests" ON)
option(HELIANTHUS_TEST_MESSAGE "Enable Message tests" ON)
option(HELIANTHUS_TEST_DISCOVERY "Enable Discovery tests" ON)
option(HELIANTHUS_TEST_DATABASE "Enable Database tests" OFF)
option(HELIANTHUS_TEST_INTEGRATION "Enable Integration tests" OFF)
option(HELIANTHUS_TEST_NETWORK "Enable Network tests" OFF)
option(HELIANTHUS_TEST_SCRIPTING "Enable Scripting tests" OFF)

# 添加子目录
if(HELIANTHUS_TEST_COMMON)
    add_subdirectory(Common)
endif()

if(HELIANTHUS_TEST_CONFIG)
    add_subdirectory(Config)
endif()

if(HELIANTHUS_TEST_MESSAGE)
    add_subdirectory(Message)
endif()

if(HELIANTHUS_TEST_DISCOVERY)
    add_subdirectory(Discovery)
endif()

if(HELIANTHUS_TEST_DATABASE)
    add_subdirectory(Database)
endif()

if(HELIANTHUS_TEST_INTEGRATION)
    add_subdirectory(Integration)
endif()

if(HELIANTHUS_TEST_NETWORK)
    add_subdirectory(Network)
endif()

if(HELIANTHUS_TEST_SCRIPTING)
    add_subdirectory(Scripting)
endif()

# 注意：不需要在这里添加测试，子目录中的测试会自动被发现
# 如果需要运行所有测试，请使用 ctest 命令或我们创建的 run_tests.sh 脚本