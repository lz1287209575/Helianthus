cc_library(
    name = "spdlog",
    hdrs = glob([
        "include/**/*.h",
    ]),
    includes = ["include"],
    visibility = ["//visibility:public"],
    copts = [
        "-DSPDLOG_COMPILED_LIB",
        "-std=c++11",
    ],
    linkopts = ["-pthread"],
)