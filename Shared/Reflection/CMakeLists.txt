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
set(GENERATED_CLASSES_DIR ${GENERATED_DIR}/classes)

# 生成的文件列表
set(GENERATED_REFLECTION_FILES
    ${GENERATED_DIR}/reflection_gen.h
    ${GENERATED_DIR}/reflection_registrations.cpp
    ${GENERATED_DIR}/reflection_services.cpp
    ${GENERATED_DIR}/reflection_auto_mount.cpp
    ${GENERATED_DIR}/reflection_init.cpp
)

add_custom_command(
    OUTPUT ${GENERATED_REFLECTION_FILES}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/reflection_codegen.py
            ${CMAKE_SOURCE_DIR} ${GENERATED_DIR}
    # Re-run when any headers/sources change
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/reflection_codegen.py
            ${CMAKE_SOURCE_DIR}/Shared
            ${CMAKE_SOURCE_DIR}/Examples
    COMMENT "Generating reflection registration code"
    VERBATIM
)

# 显式自定义目标，便于其他目标声明依赖，避免隐式依赖引发的循环
add_custom_target(generate_reflection ALL
    DEPENDS ${GENERATED_REFLECTION_FILES}
)

add_library(helianthus_reflection_gen STATIC 
    ${GENERATED_DIR}/reflection_registrations.cpp
    ${GENERATED_DIR}/reflection_services.cpp
    ${GENERATED_DIR}/reflection_auto_mount.cpp
    ${GENERATED_DIR}/reflection_init.cpp
)
add_dependencies(helianthus_reflection_gen generate_reflection)
target_include_directories(helianthus_reflection_gen PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
)
target_link_libraries(helianthus_reflection_gen PUBLIC helianthus_common helianthus_rpc nlohmann_json::nlohmann_json)
target_compile_features(helianthus_reflection_gen PUBLIC cxx_std_20)

# 可选：逐类对象文件（便于增量编译）。直接把 classes 目录下的所有生成片段编进库，
# CMake 会按文件粒度增量重建，减少链接成本。
file(GLOB_RECURSE GENERATED_CLASS_PARTS
    ${GENERATED_CLASSES_DIR}/*_registration.cpp
    ${GENERATED_CLASSES_DIR}/*_services.cpp)

if(GENERATED_CLASS_PARTS)
    target_sources(helianthus_reflection_gen PRIVATE ${GENERATED_CLASS_PARTS})
endif()

# 进一步：为每个类创建 OBJECT 库，便于最小化增量重建与链接
file(GLOB_RECURSE GENERATED_CLASS_REGS ${GENERATED_CLASSES_DIR}/*/*_registration.cpp ${GENERATED_CLASSES_DIR}/*_registration.cpp)
foreach(REG_FILE IN LISTS GENERATED_CLASS_REGS)
    get_filename_component(REG_DIR  ${REG_FILE} DIRECTORY)
    get_filename_component(BASE_WE  ${REG_FILE} NAME_WE) # e.g. Class_registration
    string(REPLACE "_registration" "" CLASS_NAME ${BASE_WE})
    set(SVC_FILE ${REG_DIR}/${CLASS_NAME}_services.cpp)
    if(EXISTS ${SVC_FILE})
        # Sanitize object lib name: replace path separators with underscores
        file(RELATIVE_PATH REL_FROM_GEN ${GENERATED_CLASSES_DIR} ${REG_DIR})
        string(REPLACE "/" "_" REL_FROM_GEN_SAN ${REL_FROM_GEN})
        set(OBJ_LIB_NAME hel_refl_obj_${REL_FROM_GEN_SAN}_${CLASS_NAME})
        add_library(${OBJ_LIB_NAME} OBJECT ${REG_FILE} ${SVC_FILE})
        add_dependencies(${OBJ_LIB_NAME} generate_reflection)
        target_include_directories(${OBJ_LIB_NAME} PUBLIC
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
            $<BUILD_INTERFACE:${GENERATED_DIR}>)
        target_compile_features(${OBJ_LIB_NAME} PUBLIC cxx_std_20)
        target_link_libraries(${OBJ_LIB_NAME} PUBLIC nlohmann_json::nlohmann_json helianthus_common helianthus_rpc)
        # Link OBJECTS into the aggregate static lib
        target_sources(helianthus_reflection_gen PRIVATE $<TARGET_OBJECTS:${OBJ_LIB_NAME}>)
    endif()
endforeach()


