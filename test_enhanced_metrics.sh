#!/bin/bash

echo "=== 测试增强的 Prometheus 指标导出功能 ==="

# 启动增强的 Prometheus 导出器
echo "启动增强的 Prometheus 导出器..."
bazel-bin/Examples/enhanced_prometheus_example &
EXPORTER_PID=$!

# 等待程序启动
sleep 5

echo "等待程序启动完成..."

# 测试基础指标
echo -e "\n=== 基础指标测试 ==="
curl -s http://localhost:9109/metrics | grep -E "helianthus_queue_|helianthus_batch_|helianthus_zero_copy_|helianthus_tx_" | head -10

# 测试增强的批处理指标
echo -e "\n=== 增强批处理指标测试 ==="
curl -s http://localhost:9109/metrics | grep "helianthus_batch_duration_p" | head -5

# 测试增强的零拷贝指标
echo -e "\n=== 增强零拷贝指标测试 ==="
curl -s http://localhost:9109/metrics | grep "helianthus_zero_copy_duration_p" | head -5

# 测试增强的事务指标
echo -e "\n=== 增强事务指标测试 ==="
curl -s http://localhost:9109/metrics | grep "helianthus_transaction_" | head -10

# 测试直方图指标
echo -e "\n=== 直方图指标测试 ==="
curl -s http://localhost:9109/metrics | grep "helianthus_batch_duration_ms_bucket" | head -5

# 测试 HELP 和 TYPE 注释
echo -e "\n=== HELP 和 TYPE 注释测试 ==="
curl -s http://localhost:9109/metrics | grep -E "# HELP|# TYPE" | grep -E "batch|zero_copy|transaction" | head -10

# 停止程序
echo -e "\n停止程序..."
kill $EXPORTER_PID
wait $EXPORTER_PID 2>/dev/null

echo -e "\n=== 测试完成 ==="
