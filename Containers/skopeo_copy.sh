#!/usr/bin/env bash
set -euo pipefail

# 用法:
#   SRC=containers-storage:helianthus:latest \
#   DST=docker://registry.example.com/helianthus:latest \
#   ./Containers/skopeo_copy.sh

SRC="${SRC:-containers-storage:helianthus:latest}"
DST="${DST:-}"

if ! command -v skopeo >/dev/null 2>&1; then
  echo "skopeo 未安装，无法复制/推送镜像。"
  exit 1
fi

if [ -z "${DST}" ]; then
  echo "请设置目标 DST，例如: docker://registry.example.com/helianthus:latest"
  exit 1
fi

echo "[skopeo] 复制镜像: ${SRC} -> ${DST}"
skopeo copy "${SRC}" "${DST}"
echo "完成。"


