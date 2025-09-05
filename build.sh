#!/bin/bash
set -e

# Helianthus CMake + Conan 构建脚本

echo "🚀 开始构建 Helianthus 项目..."

# 检查依赖
check_dependency() {
    if ! command -v $1 &> /dev/null; then
        echo "❌ 错误: $1 未安装"
        echo "请先安装 $1"
        exit 1
    fi
}

echo "📋 检查构建依赖..."
check_dependency "conan"
check_dependency "cmake"
check_dependency "ninja"

# 设置构建目录
BUILD_DIR="build"
INSTALL_DIR="install"

# 清理旧的构建
if [ -d "$BUILD_DIR" ]; then
    echo "🧹 清理旧的构建目录..."
    rm -rf "$BUILD_DIR"
fi

if [ -d "$INSTALL_DIR" ]; then
    echo "🧹 清理旧的安装目录..."
    rm -rf "$INSTALL_DIR"
fi

# 创建构建目录
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "📦 安装 Conan 依赖..."
# 配置 Conan 使用国内镜像源
conan remote add conan-center https://mirrors.tuna.tsinghua.edu.cn/conan/conan-center || true
conan remote add bincrafters https://mirrors.tuna.tsinghua.edu.cn/conan/bincrafters || true

# 安装依赖
conan install .. --build=missing --settings=build_type=Release

echo "🔨 配置 CMake..."
# 配置 CMake
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
    -DCMAKE_GENERATOR=Ninja \
    -DHELIANTHUS_BUILD_TESTS=ON \
    -DHELIANTHUS_BUILD_EXAMPLES=ON \
    -DHELIANTHUS_ENABLE_LOGGING=ON \
    -DHELIANTHUS_USE_TCMALLOC=ON

echo "🏗️ 开始编译..."
# 编译项目
cmake --build . --config Release --parallel $(nproc)

echo "🧪 运行测试..."
# 运行测试
ctest --output-on-failure --parallel $(nproc)

echo "📦 安装项目..."
# 安装项目
cmake --install . --prefix ../$INSTALL_DIR

echo "✅ 构建完成！"
echo "📁 安装目录: $INSTALL_DIR"
echo "🚀 可执行文件位于: $INSTALL_DIR/bin"
echo "📚 库文件位于: $INSTALL_DIR/lib"
echo "📖 头文件位于: $INSTALL_DIR/include"

# 显示构建信息
echo ""
echo "📊 构建信息:"
echo "  - 构建类型: Release"
echo "  - 编译器: $(gcc --version | head -n1)"
echo "  - CMake 版本: $(cmake --version | head -n1)"
echo "  - Conan 版本: $(conan --version)"
echo "  - 并行编译: $(nproc) 个线程"


