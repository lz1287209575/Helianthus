# Security Scan Setup Guide

## 问题描述

GitHub Actions 中的 Security Scan 步骤卡住，主要错误包括：
1. CPE 分析器初始化失败
2. 无法读取 suppression.xml 文件

## 解决方案

### 1. 创建的文件

- `suppression.xml` - OWASP Dependency Check 抑制文件
- `.github/workflows/security-scan.yml` - 完整的安全扫描工作流
- `.github/workflows/security-scan-simple.yml` - 简化的安全扫描工作流
- `.github/workflows/ci.yml` - 基础 CI 工作流
- `dependency-check-config.xml` - OWASP Dependency Check 配置文件

### 2. 推荐使用的工作流

**对于 C++ 项目，推荐使用 `security-scan-simple.yml`**，因为它：
- 避免了 OWASP Dependency Check 的复杂性问题
- 包含基本的 C++ 安全检查
- 运行时间更短
- 更适合 C++ 项目的特性

### 3. 如何启用

1. 将 `.github/workflows/security-scan-simple.yml` 重命名为 `security-scan.yml`
2. 删除或重命名其他工作流文件以避免冲突
3. 提交更改到仓库

### 4. 安全检查内容

简化的安全扫描包括：
- ✅ 构建验证
- ✅ 危险 C 函数检查 (strcpy, sprintf, gets, scanf)
- ✅ 硬编码密钥检查
- ✅ 缓冲区操作检查
- ⚠️ OWASP Dependency Check (如果可用)

### 5. 故障排除

如果仍然遇到问题：

1. **检查 suppression.xml 文件**：
   ```bash
   # 确保文件存在且格式正确
   cat suppression.xml
   ```

2. **手动运行依赖检查**：
   ```bash
   # 安装 OWASP Dependency Check
   wget https://github.com/jeremylong/DependencyCheck/releases/latest/download/dependency-check-8.4.0-release.zip
   unzip dependency-check-8.4.0-release.zip
   
   # 运行检查
   ./dependency-check/bin/dependency-check.sh \
     --project "Helianthus" \
     --scan . \
     --suppression suppression.xml \
     --format HTML \
     --out reports/
   ```

3. **使用替代方案**：
   - 使用 `security-scan-simple.yml` 进行基本安全检查
   - 考虑使用其他 C++ 安全扫描工具

### 6. 自定义配置

可以根据项目需要修改：
- `suppression.xml` - 添加更多抑制规则
- 工作流文件 - 调整检查规则和阈值
- `dependency-check-config.xml` - 配置 OWASP Dependency Check

## 注意事项

- OWASP Dependency Check 主要针对 Java/JavaScript 项目，对 C++ 项目支持有限
- 建议结合其他 C++ 安全工具使用
- 定期更新抑制文件以处理新的误报



