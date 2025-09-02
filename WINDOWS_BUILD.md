# Windows 构建支持

## 概述

本项目现在支持在Windows环境下构建，通过GitHub Actions提供跨平台CI/CD支持。

## 支持的平台

### 操作系统
- **Windows**: `windows-latest` (Windows Server 2022)
- **Linux**: `ubuntu-latest`, `ubuntu-22.04`, `ubuntu-24.04`

### 编译器
- **Windows**: MSVC (Microsoft Visual C++)
- **Linux**: GCC, Clang

## Windows构建矩阵

### CI/CD主流程
```yaml
matrix:
  os: [ubuntu-latest, ubuntu-22.04, ubuntu-24.04, windows-latest]
  compiler: [gcc, clang, msvc]
  include:
    - os: windows-latest
      compiler: msvc
      bazel_platform: windows
```

### 快速构建
```yaml
matrix:
  os: [ubuntu-latest, windows-latest]
```

### 代码质量检查
```yaml
matrix:
  os: [ubuntu-latest, windows-latest]
```

## Windows环境特性

### 1. 编译器配置
- **MSVC**: 使用Microsoft Visual C++编译器
- **工具链**: 自动检测和配置Windows SDK
- **标准库**: 使用MSVC标准库

### 2. 路径处理
- **缓存路径**: `C:\Users\runneradmin\.cache\bazel`
- **Bazel路径**: `C:\Users\runneradmin\.bazel`
- **依赖路径**: `C:\Users\runneradmin\.cache\bazel\_bazel_runneradmin\external` 

### 3. 依赖管理
- **系统依赖**: Windows环境通常已包含必要工具
- **包管理**: 使用Windows内置包管理器
- **构建工具**: 自动检测可用的构建工具

## 构建流程

### 1. 环境检测
```yaml
- name: Setup C++ toolchain
  run: |
    if [ "${{ matrix.os }}" = "windows-latest" ]; then
      # Windows环境使用MSVC
      echo "Using MSVC on Windows"
      echo "CC=cl.exe" >> $GITHUB_ENV
      echo "CXX=cl.exe" >> $GITHUB_ENV
    fi
```

### 2. 依赖安装
```yaml
- name: Install Windows dependencies
  if: matrix.os == 'windows-latest'
  run: |
    # Windows环境通常已经包含了必要的工具
    echo "Windows environment setup completed"
```

### 3. 缓存配置
```yaml
- name: Cache Bazel
  uses: actions/cache@v4
  with:
    path: |
      ${{ matrix.os == 'windows-latest' && 'C:\Users\runneradmin\.cache\bazel' || '~/.cache/bazel' }}
```

## 本地Windows构建

### 1. 环境要求
- **Windows 10/11** 或 **Windows Server 2019/2022**
- **Visual Studio 2019/2022** 或 **Build Tools**
- **Bazel 8.4.0+**
- **Git for Windows**

### 2. 安装步骤

#### 安装Bazel
```powershell
# 使用Chocolatey
choco install bazel

# 或下载安装包
# https://github.com/bazelbuild/bazel/releases
```

#### 安装Visual Studio
```powershell
# 安装Visual Studio Community (免费)
# 或安装Build Tools (仅构建工具)
```

#### 安装Git
```powershell
# 下载Git for Windows
# https://git-scm.com/download/win
```

### 3. 构建命令
```powershell
# 克隆项目
git clone https://github.com/your-repo/helianthus.git
cd helianthus

# 构建项目
bazel build //...

# 运行测试
bazel test //Tests/...

# 运行示例
bazel run //Examples:config_example
```

## 平台特定配置

### 1. .bazelrc配置
```bash
# Windows特定配置
build:windows --cxxopt=/std:c++20
build:windows --host_cxxopt=/std:c++20
build:windows --cxxopt=/utf-8
build:windows --host_cxxopt=/utf-8

# Linux特定配置
build:linux --cxxopt=-std=c++20
build:linux --host_cxxopt=-std:c++20
```

### 2. 条件编译
```cpp
#ifdef _WIN32
    // Windows特定代码
    #include <windows.h>
#else
    // Linux/Unix特定代码
    #include <unistd.h>
#endif
```

### 3. 路径处理
```cpp
#ifdef _WIN32
    std::string path = "C:\\path\\to\\file";
#else
    std::string path = "/path/to/file";
#endif
```

## 常见问题

### 1. 路径分隔符
- **Windows**: 使用反斜杠 `\` 或正斜杠 `/`
- **Linux**: 使用正斜杠 `/`
- **建议**: 使用正斜杠 `/` 作为跨平台标准

### 2. 文件权限
- **Windows**: 文件权限基于ACL
- **Linux**: 文件权限基于Unix权限
- **建议**: 使用相对路径避免权限问题

### 3. 环境变量
- **Windows**: `%VARIABLE%` 或 `$env:VARIABLE`
- **Linux**: `$VARIABLE`
- **Bazel**: 统一使用 `$VARIABLE` 格式

### 4. 依赖库
- **Windows**: `.lib` 和 `.dll` 文件
- **Linux**: `.a` 和 `.so` 文件
- **Bazel**: 自动处理平台差异

## 性能优化

### 1. 缓存策略
- **本地缓存**: 使用Bazel本地缓存
- **远程缓存**: 配置远程构建缓存
- **增量构建**: 利用Bazel的增量构建特性

### 2. 并行构建
- **CPU核心**: 使用所有可用CPU核心
- **内存管理**: 合理配置内存限制
- **网络优化**: 优化依赖下载

### 3. 构建配置
```bash
# 启用并行构建
bazel build --jobs=8 //...

# 启用远程缓存
bazel build --remote_cache=grpc://cache.example.com //...

# 优化内存使用
bazel build --host_jvm_args=-Xmx4g //...
```

## 测试策略

### 1. 单元测试
- **跨平台测试**: 确保在所有平台上运行
- **平台特定测试**: 针对Windows特性进行测试
- **集成测试**: 测试跨平台兼容性

### 2. 持续集成
- **自动化测试**: GitHub Actions自动运行
- **多平台验证**: 同时测试多个平台
- **回归测试**: 防止新功能破坏现有功能

### 3. 测试报告
- **覆盖率报告**: 生成代码覆盖率报告
- **性能基准**: 比较不同平台的性能
- **错误追踪**: 记录和追踪平台特定问题

## 维护和更新

### 1. 定期更新
- **Bazel版本**: 定期更新到最新版本
- **依赖版本**: 更新第三方依赖
- **工具链**: 更新编译器和工具

### 2. 兼容性检查
- **API兼容性**: 确保API在不同平台上一致
- **行为一致性**: 验证功能在不同平台上行为一致
- **性能一致性**: 确保性能在不同平台上可接受

### 3. 文档更新
- **构建说明**: 更新构建文档
- **故障排除**: 记录常见问题和解决方案
- **最佳实践**: 分享跨平台开发经验

## 参考资料

- [Bazel Windows支持](https://bazel.build/install/windows)
- [Visual Studio文档](https://docs.microsoft.com/visualstudio/)
- [Windows开发指南](https://docs.microsoft.com/windows/dev-environment/)
- [跨平台开发最佳实践](https://bazel.build/configure/windows)

## 联系信息

如有Windows构建相关问题，请：
1. 查看GitHub Issues
2. 检查构建日志
3. 参考Windows开发文档
4. 联系项目维护者
