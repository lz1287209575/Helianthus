# Bazel 8 迁移说明

## 概述

本项目已从Bazel 7的WORKSPACE模式迁移到Bazel 8的Bzlmod模式，以解决兼容性问题。

## 主要变更

### 1. 依赖管理方式
- **之前**: 使用`WORKSPACE`文件管理外部依赖
- **现在**: 使用`MODULE.bazel`文件管理外部依赖

### 2. 配置文件更新
- **`.bazelrc`**: 启用了`--enable_bzlmod`和`--noenable_workspace`
- **`MODULE.bazel`**: 添加了缺失的依赖项
- **`WORKSPACE`**: 重命名为`WORKSPACE.legacy`以避免冲突

### 3. 依赖版本更新
- **abseil-cpp**: 从`20230802.1`升级到`20240722.0`
- **rules_python**: 添加了`0.25.0`版本
- **Bazel版本**: 升级到`8.4.0`

## 解决的问题

### 1. Bazel 8兼容性
- 解决了"WORKSPACE file is disabled by default in Bazel 8"错误
- 启用了新的Bzlmod依赖管理系统

### 2. 依赖解析问题
- 修复了`rules_python`仓库无法解析的问题
- 解决了abseil-cpp版本冲突

### 3. 构建稳定性
- 使用更现代的依赖管理方式
- 提高了构建的可重现性

## 迁移步骤

### 步骤1: 更新.bazelrc
```bash
# 启用Bzlmod
common --enable_bzlmod

# 禁用旧版WORKSPACE
common --noenable_workspace
```

### 步骤2: 更新MODULE.bazel
- 添加缺失的依赖项
- 更新依赖版本到兼容版本
- 确保所有依赖都通过Bzlmod管理

### 步骤3: 重命名WORKSPACE
```bash
mv WORKSPACE WORKSPACE.legacy
```

### 步骤4: 更新GitHub Actions
- 升级Bazel版本到8.4.0
- 确保使用正确的工具链配置

## 验证迁移

### 1. 本地构建测试
```bash
bazel build //...
```

### 2. 依赖解析测试
```bash
bazel query --output=location "deps(//...)"
```

### 3. 测试执行
```bash
bazel test //Tests/...
```

## 注意事项

### 1. 依赖管理
- 所有新的依赖都应该添加到`MODULE.bazel`
- 避免使用`http_archive`等旧式依赖声明
- 优先使用BCR (Bazel Central Registry)中的模块

### 2. 版本兼容性
- 确保所有依赖版本都与Bazel 8兼容
- 定期更新依赖版本以获取安全修复
- 测试新版本依赖的兼容性

### 3. 回滚计划
- 保留`WORKSPACE.legacy`文件作为备份
- 如果出现问题，可以临时恢复旧配置
- 记录所有变更以便调试

## 故障排除

### 常见问题

#### 1. 依赖无法解析
```bash
# 检查MODULE.bazel语法
bazel mod deps

# 清理缓存
bazel clean --expunge
```

#### 2. 构建失败
```bash
# 检查依赖图
bazel query --output=graph "deps(//...)"

# 查看详细错误信息
bazel build --verbose_failures //...
```

#### 3. 版本冲突
```bash
# 查看依赖版本
bazel mod deps

# 检查锁定文件
cat MODULE.bazel.lock
```

## 后续计划

### 短期目标
- [x] 完成Bzlmod迁移
- [x] 修复GitHub Actions构建
- [x] 更新依赖版本

### 中期目标
- [ ] 移除所有`http_archive`依赖
- [ ] 完全迁移到BCR模块
- [ ] 优化依赖解析性能

### 长期目标
- [ ] 建立自动化依赖更新流程
- [ ] 实现依赖安全扫描
- [ ] 优化构建缓存策略

## 参考资料

- [Bazel Bzlmod官方文档](https://bazel.build/external/migration)
- [Bazel Central Registry](https://registry.bazel.build/)
- [Bazel 8发布说明](https://github.com/bazelbuild/bazel/releases)
- [依赖迁移指南](https://bazel.build/external/migration)

## 联系信息

如有问题，请：
1. 查看GitHub Issues
2. 检查构建日志
3. 参考Bazel官方文档
4. 联系项目维护者
