#!/usr/bin/env bash
set -euo pipefail

Port="${HELIANTHUS_PORT:-8080}"

# 简单 TCP 探测；若后续有 HTTP 健康端点，可替换为 curl 检查
if command -v nc >/dev/null 2>&1; then
  if nc -z 127.0.0.1 "${Port}"; then
    exit 0
  else
    exit 1
  fi
else
  # 回退：检测示例进程是否在运行
  if pgrep -f "HelianthusExample" >/dev/null 2>&1; then
    exit 0
  else
    exit 1
  fi
fi





