load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "mysql_client",
    # Header-only exposure to avoid compilation issues with MariaDB Connector/C
    srcs = [],
    hdrs = glob([
        "libmariadb/*.h",
        "include/*.h",
    ]),
    includes = ["include", "libmariadb"],
    deps = [],
    visibility = ["//visibility:public"],
)
