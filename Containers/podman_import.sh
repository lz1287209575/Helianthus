#!/usr/bin/env bash
set -euo pipefail

# 用法:
#   HELIANTHUS_TAG=${HELIANTHUS_TAG:-helianthus:latest}
#   ROOTFS_TAR=${ROOTFS_TAR:-bazel-bin/Containers/helianthus-rootfs.tar}
#   ./Containers/podman_import.sh

TAG="${HELIANTHUS_TAG:-helianthus:latest}"
ROOTFS_TAR="${ROOTFS_TAR:-bazel-bin/Containers/helianthus-rootfs.tar}"

if ! command -v podman >/dev/null 2>&1; then
  echo "podman 未安装，无法导入镜像。"
  exit 1
fi

if [ ! -f "${ROOTFS_TAR}" ]; then
  echo "未找到 rootfs tar: ${ROOTFS_TAR}，请先运行: bazel build //Containers:helianthus_rootfs_tar"
  exit 1
fi

TMPDIR=$(mktemp -d)
trap 'rm -rf "${TMPDIR}"' EXIT

echo "[podman] 解包 rootfs"
tar -C "${TMPDIR}" -xf "${ROOTFS_TAR}"

echo "[podman] 构建镜像: ${TAG}"
cat >"${TMPDIR}/Containerfile" <<'EOF'
FROM scratch
COPY . /
ENV HELIANTHUS_MODE=example
ENV HELIANTHUS_PORT=8080
EXPOSE 8080
ENTRYPOINT ["/app/entrypoint.sh"]
EOF

podman build -f "${TMPDIR}/Containerfile" -t "${TAG}" "${TMPDIR}"
echo "[podman] 本地镜像已创建: ${TAG}"





