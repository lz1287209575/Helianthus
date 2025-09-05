# Helianthus 安装指南

## 🚀 快速安装

### 1. 安装系统依赖

#### Ubuntu/Debian
```bash
# 更新包管理器
sudo apt update

# 安装基础工具
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    python3 \
    python3-pip \
    git \
    curl \
    wget

# 安装 Conan
pip3 install conan

# 验证安装
conan --version
cmake --version
ninja --version
```

#### CentOS/RHEL/Fedora
```bash
# CentOS/RHEL
sudo yum install -y \
    gcc-c++ \
    cmake \
    ninja-build \
    python3 \
    python3-pip \
    git \
    curl \
    wget

# Fedora
sudo dnf install -y \
    gcc-c++ \
    cmake \
    ninja-build \
    python3 \
    python3-pip \
    git \
    curl \
    wget

# 安装 Conan
pip3 install conan
```

#### macOS
```bash
# 使用 Homebrew
brew install cmake ninja python3

# 安装 Conan
pip3 install conan
```

#### Windows
```cmd
REM 安装 Chocolatey (如果未安装)
powershell -Command "Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"

REM 安装依赖
choco install cmake ninja python3 git

REM 安装 Conan
pip install conan
```

### 2. 配置 Conan

```bash
# 配置 Conan 使用国内镜像源
conan remote add conan-center https://mirrors.tuna.tsinghua.edu.cn/conan/conan-center
conan remote add bincrafters https://mirrors.tuna.tsinghua.edu.cn/conan/bincrafters

# 验证配置
conan remote list
```

### 3. 构建项目

```bash
# 克隆项目
git clone https://github.com/lz1287209575/Helianthus.git
cd Helianthus

# 使用构建脚本
./build.sh  # Linux/macOS
# 或
build.bat   # Windows

# 或手动构建
mkdir build && cd build
conan install .. --build=missing
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build . --parallel
```

## 🔧 高级配置

### 自定义构建选项

```bash
# 启用 Lua 脚本支持
cmake .. -DHELIANTHUS_ENABLE_LUA=ON

# 禁用测试构建
cmake .. -DHELIANTHUS_BUILD_TESTS=OFF

# 使用 Debug 构建
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 自定义安装目录
cmake --install . --prefix /usr/local
```

### 开发环境配置

```bash
# 创建开发环境
conan install .. --build=missing -s build_type=Debug

# 配置 IDE 支持
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# 生成 compile_commands.json
ln -s build/compile_commands.json .
```

## 🐛 故障排除

### 常见问题

#### 1. Conan 安装失败
```bash
# 升级 pip
pip3 install --upgrade pip

# 使用用户安装
pip3 install --user conan

# 添加到 PATH
echo 'export PATH=$HOME/.local/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

#### 2. 依赖下载失败
```bash
# 检查网络连接
ping mirrors.tuna.tsinghua.edu.cn

# 清理 Conan 缓存
conan remove "*" --confirm

# 使用代理
conan config set proxies.http=http://proxy:port
conan config set proxies.https=http://proxy:port
```

#### 3. CMake 配置失败
```bash
# 检查 CMake 版本 (需要 3.20+)
cmake --version

# 清理构建目录
rm -rf build && mkdir build

# 使用详细输出
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON
```

#### 4. 编译错误
```bash
# 检查编译器版本
gcc --version  # 需要 GCC 9+

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

# 测试详细输出
ctest --output-on-failure --verbose
```

## 📚 开发工具

### IDE 配置

#### Visual Studio Code
```json
{
    "cmake.configureArgs": [
        "-DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake"
    ],
    "cmake.buildDirectory": "${workspaceFolder}/build"
}
```

#### CLion
1. 打开项目根目录
2. 选择 CMake 作为构建系统
3. 设置构建目录为 `build`
4. 添加 CMake 参数: `-DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake`

#### Qt Creator
1. 打开项目根目录的 CMakeLists.txt
2. 配置构建目录为 `build`
3. 添加 CMake 参数: `-DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake`

### 代码格式化

```bash
# 安装 clang-format
sudo apt install clang-format

# 格式化代码
find . -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | xargs clang-format -i
```

## 🔄 从 Bazel 迁移

如果您之前使用 Bazel 构建系统，请参考 [BUILD_CMAKE.md](BUILD_CMAKE.md) 了解迁移详情。

## 📞 获取帮助

如果您在安装过程中遇到问题：

1. 查看本文档的故障排除部分
2. 检查 GitHub Issues
3. 提交新的 Issue

---

*最后更新: 2024-12-19*


