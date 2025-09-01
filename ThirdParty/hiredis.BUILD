load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "hiredis",
    srcs = [
        "hiredis.c",
        "net.c",
        "read.c",
        "sds.c",
        "async.c",
        "alloc.c",
        "sockcompat.c",
    ],
    hdrs = [
        "hiredis.h",
        "net.h",
        "read.h",
        "sds.h",
        "async.h",
        "adapters/ae.h",
        "adapters/glib.h",
        "adapters/ivykis.h",
        "adapters/libevent.h",
        "adapters/libev.h",
        "adapters/libuv.h",
        "adapters/qt.h",
        "fmacros.h",
        "sdsalloc.h",
        "alloc.h",
        "sockcompat.h",
        "win32.h",
        "dict.h",
        "async_private.h",
    ],
    textual_hdrs = [
        "dict.c",
    ],
    includes = ["."],
    copts = select({
        "@bazel_tools//tools/cpp:msvc": [
            "/D_CRT_SECURE_NO_WARNINGS",
            "/DWIN32",
        ],
        "//conditions:default": [],
    }),
    linkopts = select({
        "@bazel_tools//tools/cpp:msvc": [
            "ws2_32.lib",
        ],
        "//conditions:default": [],
    }),
    deps = [],
    visibility = ["//visibility:public"],
)
