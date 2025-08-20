#!/bin/bash
set -e

echo "Building gperftools from source..."

# 确保脚本可执行
chmod +x autogen.sh

# 运行 autogen.sh 来生成 configure 脚本
echo "Running autogen.sh..."
./autogen.sh

# 配置构建
echo "Configuring gperftools..."
./configure \
    --enable-static \
    --disable-shared \
    --disable-libunwind \
    --enable-frame-pointers \
    --disable-debugalloc \
    --disable-heap-checker \
    --disable-heap-profiler \
    --disable-cpu-profiler \
    --disable-gperftools-realloc \
    --prefix="$INSTALLDIR"

# 编译
echo "Compiling gperftools..."
make -j$(nproc)

# 安装
echo "Installing gperftools..."
make install

echo "gperftools build completed successfully!"
