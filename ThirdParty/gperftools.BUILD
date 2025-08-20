load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

# 首先创建一个 filegroup 来包含所有源文件
filegroup(
    name = "all_srcs",
    srcs = glob(["**"], exclude=["**/*.bazel", "**/BUILD", "**/BUILD.bazel"]),
)

# 使用 configure_make 构建 gperftools
cmake(
    name = "gperftools_build",
    lib_source = ":all_srcs",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "Release",
        "BUILD_SHARED_LIBS": "ON",
        "BUILD_TESTING": "OFF",
        "GPERFTOOLS_BUILD_STATIC": "OFF",
        "GPERFTOOLS_BUILD_HEAP_CHECKER": "OFF",
        "GPERFTOOLS_BUILD_HEAP_PROFILER": "OFF",
        "GPERFTOOLS_BUILD_CPU_PROFILER": "OFF",
        "GPERFTOOLS_BUILD_DEBUGALLOC": "OFF",
    },
    env = {
        "CC": "gcc",
        "CXX": "g++",
    },
    out_static_libs = [],
    out_shared_libs = [
        "libtcmalloc_minimal.so",
    ],
    out_include_dir = "include",
    out_lib_dir = "lib64",
    visibility = ["//visibility:public"],
)

# Thin wrapper that re-exports headers/libs from foreign_cc
cc_library(
    name = "tcmalloc",
    deps = [":gperftools_build"],
    visibility = ["//visibility:public"],
)