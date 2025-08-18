load("@rules_cc//cc:defs.bzl", "cc_library")

# Note: MongoDB C++ Driver is complex to build from source in Bazel.
# This provides header-only access for now. For full functionality,
# consider using prebuilt libraries or implementing custom build rules.

cc_library(
    name = "mongocxx",
    srcs = [],
    hdrs = glob([
        "src/mongocxx/include/mongocxx/v_noabi/mongocxx/**/*.hpp",
    ]),
    includes = ["src/mongocxx/include"],
    deps = [":bsoncxx"],
    visibility = ["//visibility:public"],
    copts = select({
        "//conditions:default": [],
    }),
)

cc_library(
    name = "bsoncxx",
    srcs = [],
    hdrs = glob([
        "src/bsoncxx/include/bsoncxx/v_noabi/bsoncxx/**/*.hpp",
    ]),
    includes = ["src/bsoncxx/include"],
    deps = [],
    visibility = ["//visibility:public"],
    copts = select({
        "//conditions:default": [],
    }),
)
