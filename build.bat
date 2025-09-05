@echo off
setlocal enabledelayedexpansion

REM Helianthus CMake + Conan 构建脚本 (Windows)

echo 🚀 开始构建 Helianthus 项目...

REM 检查依赖
where conan >nul 2>nul
if %errorlevel% neq 0 (
    echo ❌ 错误: Conan 未安装
    echo 请先安装 Conan: pip install conan
    exit /b 1
)

where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo ❌ 错误: CMake 未安装
    echo 请先安装 CMake
    exit /b 1
)

where ninja >nul 2>nul
if %errorlevel% neq 0 (
    echo ❌ 错误: Ninja 未安装
    echo 请先安装 Ninja
    exit /b 1
)

echo 📋 检查构建依赖完成...

REM 设置构建目录
set BUILD_DIR=build
set INSTALL_DIR=install

REM 清理旧的构建
if exist "%BUILD_DIR%" (
    echo 🧹 清理旧的构建目录...
    rmdir /s /q "%BUILD_DIR%"
)

if exist "%INSTALL_DIR%" (
    echo 🧹 清理旧的安装目录...
    rmdir /s /q "%INSTALL_DIR%"
)

REM 创建构建目录
mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

echo 📦 安装 Conan 依赖...
REM 配置 Conan 使用国内镜像源
conan remote add conan-center https://mirrors.tuna.tsinghua.edu.cn/conan/conan-center 2>nul
conan remote add bincrafters https://mirrors.tuna.tsinghua.edu.cn/conan/bincrafters 2>nul

REM 安装依赖
conan install .. --build=missing --settings=build_type=Release

echo 🔨 配置 CMake...
REM 配置 CMake
cmake .. ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake ^
    -DCMAKE_GENERATOR=Ninja ^
    -DHELIANTHUS_BUILD_TESTS=ON ^
    -DHELIANTHUS_BUILD_EXAMPLES=ON ^
    -DHELIANTHUS_ENABLE_LOGGING=ON ^
    -DHELIANTHUS_USE_TCMALLOC=ON

echo 🏗️ 开始编译...
REM 编译项目
cmake --build . --config Release --parallel

echo 🧪 运行测试...
REM 运行测试
ctest --output-on-failure --parallel

echo 📦 安装项目...
REM 安装项目
cmake --install . --prefix ../%INSTALL_DIR%

echo ✅ 构建完成！
echo 📁 安装目录: %INSTALL_DIR%
echo 🚀 可执行文件位于: %INSTALL_DIR%\bin
echo 📚 库文件位于: %INSTALL_DIR%\lib
echo 📖 头文件位于: %INSTALL_DIR%\include

REM 显示构建信息
echo.
echo 📊 构建信息:
echo   - 构建类型: Release
echo   - 编译器: MSVC
echo   - CMake 版本: 
cmake --version | findstr "cmake version"
echo   - Conan 版本:
conan --version

pause


