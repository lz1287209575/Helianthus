#!/bin/bash

# 运行所有可用的测试
echo "=== 运行 Helianthus 测试套件 ==="
echo ""

# 进入构建目录
cd build

# 测试计数器
total_tests=0
passed_tests=0
failed_tests=0

# 运行 CommonTest
echo "1. 运行 CommonTest..."
if ./bin/tests/CommonTest; then
    echo "   ✓ CommonTest 通过"
    ((passed_tests++))
else
    echo "   ✗ CommonTest 失败"
    ((failed_tests++))
fi
((total_tests++))
echo ""

# 运行 CommandLineParserTest
echo "2. 运行 CommandLineParserTest..."
if ./bin/tests/CommandLineParserTest; then
    echo "   ✓ CommandLineParserTest 通过"
    ((passed_tests++))
else
    echo "   ✗ CommandLineParserTest 失败"
    ((failed_tests++))
fi
((total_tests++))
echo ""

# 运行 ConfigValidationTest
echo "3. 运行 ConfigValidationTest..."
if ./bin/tests/ConfigValidationTest; then
    echo "   ✓ ConfigValidationTest 通过"
    ((passed_tests++))
else
    echo "   ✗ ConfigValidationTest 失败"
    ((failed_tests++))
fi
((total_tests++))
echo ""

# 运行 TCMallocTest (如果可用)
if [ -f "./bin/tests/TCMallocTest" ]; then
    echo "4. 运行 TCMallocTest..."
    if ./bin/tests/TCMallocTest; then
        echo "   ✓ TCMallocTest 通过"
        ((passed_tests++))
    else
        echo "   ✗ TCMallocTest 失败"
        ((failed_tests++))
    fi
    ((total_tests++))
    echo ""
fi

# 运行 TCMallocRuntimeConfigTest (如果可用)
if [ -f "./bin/tests/TCMallocRuntimeConfigTest" ]; then
    echo "5. 运行 TCMallocRuntimeConfigTest..."
    if ./bin/tests/TCMallocRuntimeConfigTest; then
        echo "   ✓ TCMallocRuntimeConfigTest 通过"
        ((passed_tests++))
    else
        echo "   ✗ TCMallocRuntimeConfigTest 失败"
        ((failed_tests++))
    fi
    ((total_tests++))
    echo ""
fi

# 运行 ConfigManagerTest (如果可用)
if [ -f "./bin/tests/ConfigManagerTest" ]; then
    echo "6. 运行 ConfigManagerTest..."
    if ./bin/tests/ConfigManagerTest; then
        echo "   ✓ ConfigManagerTest 通过"
        ((passed_tests++))
    else
        echo "   ✗ ConfigManagerTest 失败"
        ((failed_tests++))
    fi
    ((total_tests++))
    echo ""
fi

# 总结
echo "=== 测试总结 ==="
echo "总测试数: $total_tests"
echo "通过: $passed_tests"
echo "失败: $failed_tests"

if [ $failed_tests -eq 0 ]; then
    echo "🎉 所有测试都通过了！"
    exit 0
else
    echo "❌ 有 $failed_tests 个测试失败"
    exit 1
fi
