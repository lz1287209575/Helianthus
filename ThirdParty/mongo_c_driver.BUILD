load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")
load("@rules_cc//cc:defs.bzl", "cc_library")

filegroup(
    name = "all_srcs",
    srcs = glob(["**"], allow_empty = True),
)

# Build MongoDB C Driver via CMake
cmake(
    name = "mongo_c_driver_build",
    lib_source = ":all_srcs",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_CXX_STANDARD": "17",
        "CMAKE_C_STANDARD": "11",
        # Use wrapper as the compiler to ensure all invocations are filtered
        "CMAKE_CXX_COMPILER": "g++",
        "CMAKE_C_COMPILER": "gcc",
        "CMAKE_CXX_COMPILER_ARG1": "g++",
        "CMAKE_C_COMPILER_ARG1": "gcc",
        "CMAKE_TRY_COMPILE_PLATFORM_VARIABLES": "CMAKE_C_COMPILER;CMAKE_CXX_COMPILER;CMAKE_C_COMPILER_ARG1;CMAKE_CXX_COMPILER_ARG1;CMAKE_C_FLAGS;CMAKE_CXX_FLAGS;CMAKE_C_STANDARD;CMAKE_CXX_STANDARD",
        "CMAKE_CXX_FLAGS": "-std=c++17",
        "CMAKE_C_FLAGS": "-O2",
        "BUILD_VERSION": "1.24.4",
        # Inject overlay to strip flags in all sub-configures/try_compile
        "CMAKE_PROJECT_TOP_LEVEL_INCLUDES": "../../ThirdParty/toolchain/cmake/overlay.cmake",
        # Simplify build; enable features as needed
        "ENABLE_SASL": "OFF",
        "ENABLE_AUTOMATIC_INIT_AND_CLEANUP": "OFF",
    },
    env = {
        # 清空/覆盖 Bazel 注入的 C++ 选项，避免 /std:c++20 传给 gcc
        "BAZEL_CXXOPTS": "-std=c++17",
        "CXX": "g++",
        "CC": "gcc",
        "CXXFLAGS": "-std=c++17",
        "CFLAGS": "-O2",
        "PATH": "$$PWD/../../ThirdParty/toolchain/bin:$$PATH",
    },
    out_shared_libs = [
        "libmongoc-1.0.so",
        "libbson-1.0.so",
    ],
    install = True,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "mongo_c_driver",
    hdrs = glob([
        "src/mongoc/**",
        "src/libbson/src/bson/**",
    ], allow_empty = True),
    includes = [
        "src",
        "src/libbson/src",
    ],
    deps = [":mongo_c_driver_build"],
    visibility = ["//visibility:public"],
)


