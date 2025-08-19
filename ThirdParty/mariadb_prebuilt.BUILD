load("@rules_cc//cc:defs.bzl", "cc_library", "cc_import")

# Expected directory structure under this external repo path (Windows):
#   include/               # headers (mysql.h, mariadb_version.h, ma_*.h ...)
#   lib/libmariadb.lib     # import library
#   bin/libmariadb.dll     # runtime DLL (optional for linking stage)

cc_import(
    name = "mariadb_bin",
    interface_library = "lib/libmariadb.lib",
    shared_library = "bin/libmariadb.dll",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "mysql_client",
    hdrs = glob(["include/**"], allow_empty = True),
    includes = ["include"],
    deps = [":mariadb_bin"],
    visibility = ["//visibility:public"],
)


