# Helianthus CMake + Conan 构建系统

## 🎯 概述

本项目已从 Bazel 迁移到 CMake + Conan 构建系统，以解决中国大陆网络环境下的依赖下载问题。

## 🚀 快速开始

### 1. 安装依赖

#### Linux/macOS
```bash
# 安装 Conan
pip install conan

# 安装 CMake 和 Ninja
sudo apt install cmake ninja-build  # Ubuntu/Debian
# 或
brew install cmake ninja            # macOS
```

#### Windows
```bash
# 安装 Conan
pip install conan

# 安装 CMake 和 Ninja
# 从官网下载安装: https://cmake.org/download/
# 从 GitHub 下载 Ninja: https://github.com/ninja-build/ninja/releases
```

### 2. 构建项目

#### Linux/macOS
```bash
# 使用构建脚本
./build.sh

# 或手动构建
mkdir build && cd build
conan install .. --build=missing
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build . --parallel
```

#### Windows
```cmd
REM 使用构建脚本
build.bat

REM 或手动构建
mkdir build && cd build
conan install .. --build=missing
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build . --parallel
```

## 📦 依赖管理

### Conan 配置

项目使用 Conan 管理依赖，配置文件位于：
- `conanfile.txt` - 简单依赖列表
- `conanfile.py` - 高级配置和选项

### 国内镜像源配置

```bash
# 添加清华大学镜像源
conan remote add conan-center https://mirrors.tuna.tsinghua.edu.cn/conan/conan-center
conan remote add bincrafters https://mirrors.tuna.tsinghua.edu.cn/conan/bincrafters

# 查看当前镜像源
conan remote list
```

### 主要依赖

| 依赖 | 版本 | 用途 |
|------|------|------|
| gtest | 1.14.0 | 单元测试 |
| spdlog | 1.12.0 | 日志系统 |
| nlohmann_json | 3.11.2 | JSON 处理 |
| zlib | 1.3 | 压缩库 |
| openssl | 3.1.4 | 加密库 |
| hiredis | 1.1.0 | Redis 客户端 |
| mongo-cxx-driver | 3.8.0 | MongoDB 客户端 |
| gperftools | 2.10 | 内存管理 |

## 🔧 构建选项

### CMake 选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `HELIANTHUS_BUILD_TESTS` | ON | 构建测试 |
| `HELIANTHUS_BUILD_EXAMPLES` | ON | 构建示例 |
| `HELIANTHUS_ENABLE_LUA` | OFF | 启用 Lua 脚本 |
| `HELIANTHUS_ENABLE_PYTHON` | OFF | 启用 Python 脚本 |
| `HELIANTHUS_ENABLE_JS` | OFF | 启用 JavaScript 脚本 |
| `HELIANTHUS_ENABLE_DOTNET` | OFF | 启用 .NET 脚本 |
| `HELIANTHUS_USE_TCMALLOC` | ON | 使用 TCMalloc |
| `HELIANTHUS_ENABLE_LOGGING` | ON | 启用日志系统 |

### 使用示例

```bash
# 启用 Lua 脚本支持
cmake .. -DHELIANTHUS_ENABLE_LUA=ON

# 禁用测试构建
cmake .. -DHELIANTHUS_BUILD_TESTS=OFF

# 使用自定义安装目录
cmake --install . --prefix /usr/local
```

## 🧪 测试

### 运行所有测试
```bash
cd build
ctest --output-on-failure --parallel
```

### 运行特定测试
```bash
cd build
./bin/tests/common_test
./bin/tests/message_queue_test
```

## 📁 项目结构

```
Helianthus/
├── CMakeLists.txt              # 根 CMake 配置
├── conanfile.txt              # Conan 依赖列表
├── conanfile.py               # Conan 高级配置
├── build.sh                   # Linux/macOS 构建脚本
├── build.bat                  # Windows 构建脚本
├── conan.conf                 # Conan 配置文件
├── cmake/                     # CMake 模块
│   └── HelianthusConfig.cmake.in
├── Shared/                    # 共享库
│   ├── CMakeLists.txt
│   ├── Common/
│   ├── Message/
│   ├── MessageQueue/
│   ├── Network/
│   ├── Config/
│   ├── Monitoring/
│   ├── Reflection/
│   ├── Scripting/
│   ├── RPC/
│   ├── Discovery/
│   ├── Database/
│   └── ThirdParty/
├── Examples/                  # 示例程序
│   └── CMakeLists.txt
└── Tests/                     # 测试程序
    └── CMakeLists.txt
```

## 🔄 从 Bazel 迁移

### 主要变更

1. **依赖管理**: 从 Bazel 的 `MODULE.bazel` 迁移到 Conan 的 `conanfile.txt`
2. **构建配置**: 从 `BUILD.bazel` 文件迁移到 `CMakeLists.txt`
3. **构建命令**: 从 `bazel build` 迁移到 `cmake --build`

### 迁移对照表

| Bazel | CMake + Conan |
|-------|---------------|
| `bazel build //...` | `cmake --build .` |
| `bazel test //...` | `ctest` |
| `bazel run //Examples:example` | `./bin/examples/example` |
| `--define=ENABLE_LUA=1` | `-DHELIANTHUS_ENABLE_LUA=ON` |

## 🐛 故障排除

### 常见问题

#### 1. Conan 依赖下载失败
```bash
# 检查网络连接
ping mirrors.tuna.tsinghua.edu.cn

# 清理 Conan 缓存
conan remove "*" --confirm

# 重新安装依赖
conan install .. --build=missing
```

#### 2. CMake 配置失败
```bash
# 检查 CMake 版本
cmake --version

# 清理构建目录
rm -rf build && mkdir build

# 重新配置
cd build && cmake ..
```

#### 3. 编译错误
```bash
# 检查编译器版本
gcc --version

# 使用详细输出
cmake --build . --verbose

# 检查依赖
conan search "*" --remote=conan-center
```

### 日志和调试

```bash
# 启用详细日志
conan install .. --build=missing -v

# CMake 详细输出
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON

# 构建详细输出
cmake --build . --verbose
```

## 📚 参考资料

- [CMake 官方文档](https://cmake.org/documentation/)
- [Conan 官方文档](https://docs.conan.io/)
- [Conan 中国镜像源](https://mirrors.tuna.tsinghua.edu.cn/conan/)
- [Ninja 构建系统](https://ninja-build.org/)

## 🤝 贡献

如果您在使用过程中遇到问题，请：

1. 查看本文档的故障排除部分
2. 检查 GitHub Issues
3. 提交新的 Issue 或 Pull Request

---

*最后更新: 2024-12-19*
*构建系统: CMake + Conan*


