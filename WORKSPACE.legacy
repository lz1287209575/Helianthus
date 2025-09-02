workspace(name = "helianthus")

# C++ toolchain and standard library
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Bazel Skylib
http_archive(
    name = "bazel_skylib",
    sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
        "https://github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
    ],
)

# Google Test
http_archive(
    name = "googletest",
    sha256 = "8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bbc5d7",
    strip_prefix = "googletest-1.12.1",
    urls = ["https://github.com/google/googletest/archive/v1.12.1.tar.gz"],
)

# Protocol Buffers
http_archive(
    name = "com_google_protobuf",
    sha256 = "3bd7828aa5af4b13b99c191e8b1e884ebfa9ad371b0ce264605d347f135d2568",
    strip_prefix = "protobuf-3.19.4",
    urls = [
        "https://github.com/protocolbuffers/protobuf/archive/v3.19.4.tar.gz",
    ],
)

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
protobuf_deps()

# spdlog for logging (using Gitee mirror)
http_archive(
    name = "spdlog",
    build_file = "//ThirdParty:spdlog.BUILD",
    sha256 = "5197b3147cfcfaa67dd564db7b878e4a4b3d9f3443801722b3915cdeced656cb",
    strip_prefix = "spdlog-1.10.0",
    urls = [
        "https://gitee.com/mirrors/spdlog/repository/archive/v1.10.0.tar.gz",
        "https://github.com/gabime/spdlog/archive/v1.10.0.tar.gz",  # fallback
    ],
)


# Add more dependencies as needed for different script engines
# Lua, Python, V8, .NET etc will be added later

# MySQL Connector/C++ (using Gitee mirror)
http_archive(
    name = "mysql_connector_cpp",
    build_file = "//ThirdParty:mysql_connector_cpp.BUILD",
    sha256 = "a0b4b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5",
    strip_prefix = "mysql-connector-cpp-8.0.33",
    urls = [
        "https://gitee.com/mirrors/mysql-connector-cpp/repository/archive/8.0.33.tar.gz",
        "https://github.com/mysql/mysql-connector-cpp/archive/8.0.33.tar.gz",  # fallback
    ],
)

# MongoDB C++ Driver (using Gitee mirror)
http_archive(
    name = "mongo_cxx_driver",
    build_file = "//ThirdParty:mongo_cxx_driver.BUILD",
    sha256 = "a0b4b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5",
    strip_prefix = "mongo-cxx-driver-r3.8.0",
    urls = [
        "https://gitee.com/mirrors/mongo-cxx-driver/repository/archive/r3.8.0.tar.gz",
        "https://github.com/mongodb/mongo-cxx-driver/archive/r3.8.0.tar.gz",  # fallback
    ],
)

# Redis C Client (hiredis) (using Gitee mirror)
http_archive(
    name = "hiredis",
    build_file = "//ThirdParty:hiredis.BUILD",
    sha256 = "a0b4b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5",
    strip_prefix = "hiredis-1.1.0",
    urls = [
        "https://gitee.com/mirrors/hiredis/repository/archive/v1.1.0.tar.gz",
        "https://github.com/redis/hiredis/archive/v1.1.0.tar.gz",  # fallback
    ],
)