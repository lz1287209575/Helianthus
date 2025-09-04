#!/usr/bin/env bash
set -euo pipefail

# 用法:
#   HELIANTHUS_TAG=${HELIANTHUS_TAG:-helianthus:latest}
#   ROOTFS_TAR=${ROOTFS_TAR:-bazel-bin/Containers/helianthus-rootfs.tar}
#   REGISTRY=${REGISTRY:-}
#   ./Containers/buildah_import.sh

TAG="${HELIANTHUS_TAG:-helianthus:latest}"
ROOTFS_TAR="${ROOTFS_TAR:-bazel-bin/Containers/helianthus-rootfs.tar}"
REGISTRY="${REGISTRY:-}"

if ! command -v buildah >/dev/null 2>&1; then
  echo "buildah 未安装，无法导入镜像。请在支持的环境中运行。"
  exit 1
fi

if [ ! -f "${ROOTFS_TAR}" ]; then
  echo "未找到 rootfs tar: ${ROOTFS_TAR}，请先运行: bazel build //Containers:helianthus_rootfs_tar"
  exit 1
fi

echo "[buildah] 从 scratch 创建镜像: ${TAG}"
CTR=$(buildah from scratch)
MNT=$(buildah mount "$CTR")

echo "[buildah] 解包 rootfs 到挂载点: ${MNT}"
tar -C "$MNT" -xf "${ROOTFS_TAR}"

echo "[buildah] 设置元数据与入口"
buildah config --env HELIANTHUS_MODE=example --env HELIANTHUS_PORT=8080 "$CTR"
buildah config --entrypoint '["/app/entrypoint.sh"]' "$CTR"
buildah config --port 8080 "$CTR"

echo "[buildah] 提交镜像标签: ${TAG}"
IMG=$(buildah commit "$CTR" "$TAG")
buildah unmount "$CTR"
buildah rm "$CTR"

if [ -n "$REGISTRY" ]; then
  echo "[buildah] 推送到仓库: ${REGISTRY}/${TAG}"
  buildah tag "$IMG" "${REGISTRY}/${TAG}"
  buildah push "${REGISTRY}/${TAG}"
else
  echo "[buildah] 本地镜像已创建: ${TAG}"
fi

echo "完成。"


