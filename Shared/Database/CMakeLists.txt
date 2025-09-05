# Database 模块 CMakeLists.txt

file(GLOB DATABASE_SOURCES "*.cpp")
file(GLOB DATABASE_HEADERS "*.h")

# 创建库
add_library(helianthus_database STATIC
    ${DATABASE_SOURCES}
    ${DATABASE_HEADERS}
)

# 设置包含目录
target_include_directories(helianthus_database PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/helianthus/Database>
)

# 链接依赖
target_link_libraries(helianthus_database PUBLIC helianthus_common helianthus_config)

# 添加可选数据库依赖
if(hiredis_FOUND)
    target_link_libraries(helianthus_database PUBLIC hiredis::hiredis)
    target_compile_definitions(helianthus_database PUBLIC HELIANTHUS_ENABLE_REDIS=1)
endif()

if(mongo-cxx-driver_FOUND)
    target_link_libraries(helianthus_database PUBLIC mongo::mongocxx_shared)
    target_compile_definitions(helianthus_database PUBLIC HELIANTHUS_ENABLE_MONGODB=1)
endif()

# 设置编译特性
target_compile_features(helianthus_database PUBLIC cxx_std_20)

# 设置输出名称
set_target_properties(helianthus_database PROPERTIES
    OUTPUT_NAME "helianthus_database"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)


