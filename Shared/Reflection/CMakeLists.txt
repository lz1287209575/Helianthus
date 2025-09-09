# Reflection 模块 CMakeLists.txt

# 源文件
set(REFLECTION_SOURCES
    Reflection.cpp
)

# 头文件
set(REFLECTION_HEADERS
    Reflection.h
    Macros.h
)

# 创建库
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})
set(GENERATED_REFLECTION ${GENERATED_DIR}/reflection_gen.cpp)

add_custom_command(
    OUTPUT ${GENERATED_REFLECTION}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/reflection_codegen.py
            ${CMAKE_SOURCE_DIR}/Shared ${GENERATED_REFLECTION}
    # Re-run when any shared headers/sources change
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/reflection_codegen.py
            ${CMAKE_SOURCE_DIR}/Shared
    COMMENT "Generating reflection registration code"
    VERBATIM
)

add_library(helianthus_reflection_gen STATIC ${GENERATED_REFLECTION})
target_include_directories(helianthus_reflection_gen PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
)
target_link_libraries(helianthus_reflection_gen PUBLIC helianthus_common helianthus_rpc)
target_compile_features(helianthus_reflection_gen PUBLIC cxx_std_20)


