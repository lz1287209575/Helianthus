cmake_minimum_required(VERSION 3.20)

# 项目信息
project(Helianthus
    VERSION 1.0.0
    DESCRIPTION "High-performance microservice game server architecture"
    LANGUAGES CXX C
)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置编译选项
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++20 /utf-8")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /W4")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 项目选项
option(HELIANTHUS_ENABLE_LUA "Enable Lua scripting support" OFF)
option(HELIANTHUS_ENABLE_PYTHON "Enable Python scripting support" OFF)
option(HELIANTHUS_ENABLE_JS "Enable JavaScript scripting support" OFF)
option(HELIANTHUS_ENABLE_DOTNET "Enable .NET scripting support" OFF)
option(HELIANTHUS_USE_TCMALLOC "Use TCMalloc for memory management" ON)
option(HELIANTHUS_ENABLE_LOGGING "Enable logging system" ON)
option(HELIANTHUS_BUILD_TESTS "Build tests" ON)
option(HELIANTHUS_BUILD_EXAMPLES "Build examples" ON)
option(HELIANTHUS_REFLECTION_SKIP_FACTORY_AUTO_REGISTER "Skip auto-registering reflection factories (use manual or class tags)" OFF)

# 设置预处理器定义
if(HELIANTHUS_ENABLE_LUA)
    add_compile_definitions(HELIANTHUS_ENABLE_LUA=1)
endif()

if(HELIANTHUS_ENABLE_PYTHON)
    add_compile_definitions(HELIANTHUS_ENABLE_PYTHON=1)
endif()

if(HELIANTHUS_ENABLE_JS)
    add_compile_definitions(HELIANTHUS_ENABLE_JS=1)
endif()

if(HELIANTHUS_ENABLE_DOTNET)
    add_compile_definitions(HELIANTHUS_ENABLE_DOTNET=1)
endif()

if(HELIANTHUS_USE_TCMALLOC)
    add_compile_definitions(HELIANTHUS_USE_TCMALLOC=1)
endif()

if(HELIANTHUS_ENABLE_LOGGING)
    add_compile_definitions(HELIANTHUS_ENABLE_LOGGING=1)
endif()

if(HELIANTHUS_REFLECTION_SKIP_FACTORY_AUTO_REGISTER)
    add_compile_definitions(HELIANTHUS_REFLECTION_SKIP_FACTORY_AUTO_REGISTER=1)
endif()

# 包含目录
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/Shared)
include_directories(${CMAKE_SOURCE_DIR}/ThirdParty)

# Conan 集成
if(NOT CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR "Please, run Conan first to generate the toolchain file.")
endif()
include(${CMAKE_TOOLCHAIN_FILE})

# 设置 Conan 生成的配置文件路径
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/cmake/conan)

# 查找 Conan 依赖
find_package(PkgConfig REQUIRED)

# 查找必需的包
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(GTest REQUIRED)

# 查找可选包
find_package(hiredis QUIET)
find_package(mongo-cxx-driver QUIET)
find_package(gperftools QUIET)
find_package(LZ4 QUIET)
find_package(Zstd QUIET)
find_package(Snappy QUIET)
find_package(CURL QUIET)

# 设置链接库
set(HELIANTHUS_LIBS
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ZLIB::ZLIB
    OpenSSL::SSL
    OpenSSL::Crypto
)

# 添加可选依赖
if(hiredis_FOUND)
    list(APPEND HELIANTHUS_LIBS hiredis::hiredis)
    add_compile_definitions(HELIANTHUS_ENABLE_REDIS=1)
endif()

if(mongo-cxx-driver_FOUND)
    list(APPEND HELIANTHUS_LIBS mongo::mongocxx_shared)
    add_compile_definitions(HELIANTHUS_ENABLE_MONGODB=1)
endif()

if(gperftools_FOUND AND HELIANTHUS_USE_TCMALLOC)
    list(APPEND HELIANTHUS_LIBS gperftools::tcmalloc)
endif()

if(LZ4_FOUND)
    list(APPEND HELIANTHUS_LIBS LZ4::lz4_static)
    add_compile_definitions(HELIANTHUS_ENABLE_LZ4=1)
endif()

if(Zstd_FOUND)
    list(APPEND HELIANTHUS_LIBS zstd::libzstd_static)
    add_compile_definitions(HELIANTHUS_ENABLE_ZSTD=1)
endif()

if(Snappy_FOUND)
    list(APPEND HELIANTHUS_LIBS Snappy::Snappy)
    add_compile_definitions(HELIANTHUS_ENABLE_SNAPPY=1)
endif()

if(CURL_FOUND)
    list(APPEND HELIANTHUS_LIBS CURL::libcurl)
    add_compile_definitions(HELIANTHUS_ENABLE_CURL=1)
endif()

# 系统库
if(UNIX AND NOT APPLE)
    list(APPEND HELIANTHUS_LIBS pthread dl rt)
elseif(WIN32)
    list(APPEND HELIANTHUS_LIBS ws2_32 crypt32)
endif()

# 添加子目录
add_subdirectory(Shared)

# 添加示例
if(HELIANTHUS_BUILD_EXAMPLES)
    add_subdirectory(Examples)
endif()

# 添加测试
if(HELIANTHUS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

# 安装配置
install(TARGETS helianthus_common helianthus_message helianthus_message_queue helianthus_config helianthus_third_party helianthus_database helianthus_network helianthus_discovery helianthus_scripting helianthus_rpc
    EXPORT HelianthusTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY Shared/
    DESTINATION include/helianthus
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

install(EXPORT HelianthusTargets
    FILE HelianthusTargets.cmake
    NAMESPACE Helianthus::
    DESTINATION lib/cmake/Helianthus
)

# 创建配置文件
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/HelianthusConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/HelianthusConfig.cmake"
    INSTALL_DESTINATION lib/cmake/Helianthus
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/HelianthusConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/HelianthusConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/HelianthusConfigVersion.cmake"
    DESTINATION lib/cmake/Helianthus
)

# 打印配置信息
message(STATUS "Helianthus Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Enable Lua: ${HELIANTHUS_ENABLE_LUA}")
message(STATUS "  Enable Python: ${HELIANTHUS_ENABLE_PYTHON}")
message(STATUS "  Enable JavaScript: ${HELIANTHUS_ENABLE_JS}")
message(STATUS "  Enable .NET: ${HELIANTHUS_ENABLE_DOTNET}")
message(STATUS "  Use TCMalloc: ${HELIANTHUS_USE_TCMALLOC}")
message(STATUS "  Enable Logging: ${HELIANTHUS_ENABLE_LOGGING}")
message(STATUS "  Build Tests: ${HELIANTHUS_BUILD_TESTS}")
message(STATUS "  Build Examples: ${HELIANTHUS_BUILD_EXAMPLES}")
