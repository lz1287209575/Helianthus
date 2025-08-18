load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "hiredis",
    srcs = [
        "hiredis.c",
        "net.c", 
        "read.c",
        "sds.c",
        "async.c",
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
    ],
    includes = ["."],
    deps = [],
    visibility = ["//visibility:public"],
    strip_include_prefix = "",
    copts = select({
        "//conditions:default": [],
    }),
)
