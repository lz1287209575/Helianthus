#!/bin/bash

# Helianthus API 文档生成脚本

set -e

echo "🚀 开始生成 Helianthus API 文档..."

# 检查依赖
check_dependencies() {
    echo "📋 检查依赖..."
    if ! command -v doxygen &> /dev/null; then
        echo "❌ Doxygen 未安装，请运行: sudo apt-get install doxygen graphviz"
        exit 1
    fi
    
    if ! command -v dot &> /dev/null; then
        echo "❌ Graphviz 未安装，请运行: sudo apt-get install graphviz"
        exit 1
    fi
}

# 创建 Doxygen 配置
create_doxyfile() {
    echo "⚙️ 创建 Doxygen 配置..."
    cat > Doxyfile << 'EOF'
PROJECT_NAME = "Helianthus API"
PROJECT_NUMBER = "1.0"
PROJECT_BRIEF = "高性能、可扩展的微服务游戏服务器架构"
OUTPUT_DIRECTORY = docs/doxygen
GENERATE_HTML = YES
GENERATE_LATEX = NO
EXTRACT_ALL = YES
EXTRACT_PRIVATE = YES
EXTRACT_STATIC = YES
HAVE_DOT = YES
UML_LOOK = YES
CALL_GRAPH = YES
CALLER_GRAPH = YES
INPUT = Shared/
FILE_PATTERNS = *.h *.cpp *.hpp
RECURSIVE = YES
EXCLUDE_PATTERNS = */ThirdParty/* */Tests/*
HTML_OUTPUT = html
HTML_FILE_EXTENSION = .html
HTML_HEADER = 
HTML_FOOTER = 
HTML_STYLESHEET = 
HTML_EXTRA_STYLESHEET = 
HTML_EXTRA_FILES = 
HTML_COLORSTYLE_HUE = 220
HTML_COLORSTYLE_SAT = 100
HTML_COLORSTYLE_GAMMA = 80
HTML_TIMESTAMP = YES
HTML_DYNAMIC_SECTIONS = NO
GENERATE_LATEX = NO
EOF
}

# 生成文档
generate_docs() {
    echo "📚 生成 API 文档..."
    doxygen Doxyfile
    
    if [ $? -eq 0 ]; then
        echo "✅ API 文档生成成功！"
        echo "📖 文档位置: docs/doxygen/html/index.html"
    else
        echo "❌ 文档生成失败"
        exit 1
    fi
}

# 创建文档首页
create_index() {
    echo "🏠 创建文档首页..."
    cat > docs/README.md << 'EOF'
# Helianthus 文档

## 📚 API 文档

- [API 参考](doxygen/html/index.html) - 完整的 API 文档
- [用户指南](guides/) - 使用指南和教程
- [示例代码](examples/) - 代码示例

## 🚀 快速开始

1. [安装指南](guides/installation.md)
2. [配置说明](guides/configuration.md)
3. [部署指南](guides/deployment.md)

## 📖 功能文档

- [消息队列](guides/message-queue.md)
- [分布式事务](guides/distributed-transactions.md)
- [性能监控](guides/monitoring.md)
- [压缩加密](guides/compression-encryption.md)

## 🔧 开发文档

- [架构设计](guides/architecture.md)
- [扩展开发](guides/extension.md)
- [测试指南](guides/testing.md)

## 📊 性能报告

- [性能基准测试](reports/performance-benchmark.md)
- [分布式事务验证](reports/distributed-transaction-validation.md)

---

**最后更新**: 2025年09月
**版本**: 1.0
EOF
}

# 主函数
main() {
    check_dependencies
    create_doxyfile
    generate_docs
    create_index
    
    echo "🎉 文档生成完成！"
    echo "📖 访问文档: docs/doxygen/html/index.html"
}

main "$@"
