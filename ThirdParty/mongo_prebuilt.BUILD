load("@rules_cc//cc:defs.bzl", "cc_library", "cc_import")

# Expected directory structure under this external repo path (Windows, prebuilt scenario):
#   include/                               # mongo-cxx-driver headers root (mongocxx/, bsoncxx/)
#   lib/mongocxx.lib
#   lib/bsoncxx.lib
#   bin/mongocxx.dll                       # optional at build time
#   bin/bsoncxx.dll                        # optional at build time
#
# NOTE: mongo-cxx-driver depends on mongo-c-driver (libmongoc-1.0, libbson-1.0).
# If using fully prebuilt bundle, ensure their symbols are resolved inside mongocxx/bsoncxx libs.

cc_import(
    name = "bsoncxx_bin",
    interface_library = "lib/bsoncxx.lib",
    shared_library = "bin/bsoncxx.dll",
    visibility = ["//visibility:public"],
)

cc_import(
    name = "mongocxx_bin",
    interface_library = "lib/mongocxx.lib",
    shared_library = "bin/mongocxx.dll",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "bsoncxx",
    hdrs = glob(["include/bsoncxx/**"], allow_empty = True),
    includes = ["include"],
    deps = [":bsoncxx_bin"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "mongocxx",
    hdrs = glob(["include/mongocxx/**"], allow_empty = True),
    includes = ["include"],
    deps = [":mongocxx_bin", ":bsoncxx"],
    visibility = ["//visibility:public"],
)


