# Containers

本目录提供 Helianthus 的容器化构建与运行示例。

## 目标
- 多阶段构建，最小化运行镜像体积
- 与 Bazel 集成，支持 `bazel build //...`
- 可配置入口与健康检查

## 快速开始

```bash
# 1) 构建镜像（包含示例应用）
docker build -f Containers/Dockerfile -t helianthus:latest .

# 2) 运行（示例：以示例程序启动）
docker run --rm \
  -e HELIANTHUS_MODE=example \
  -e HELIANTHUS_PORT=8080 \
  -p 8080:8080 \
  --name helianthus \
  helianthus:latest

# 3) 健康检查（容器内已设置 HEALTHCHECK）
docker inspect --format='{{json .State.Health}}' helianthus | jq
```

## 无 Docker 环境的替代流程

### 方案A：Bazel 生成 rootfs tar
```bash
bazel build //Containers:helianthus_rootfs_tar
ls -lh bazel-bin/Containers/helianthus-rootfs.tar
```

### 方案B：使用 buildah 从 rootfs tar 构造镜像
```bash
HELIANTHUS_TAG=helianthus:latest \
ROOTFS_TAR=bazel-bin/Containers/helianthus-rootfs.tar \
./Containers/buildah_import.sh

# 可选推送
REGISTRY=registry.example.com \
HELIANTHUS_TAG=helianthus:latest \
ROOTFS_TAR=bazel-bin/Containers/helianthus-rootfs.tar \
./Containers/buildah_import.sh
```

### 方案C：使用 podman 构建（无守护进程）
```bash
HELIANTHUS_TAG=helianthus:latest \
ROOTFS_TAR=bazel-bin/Containers/helianthus-rootfs.tar \
./Containers/podman_import.sh
```

### 方案D：使用 skopeo 复制/推送镜像
```bash
# 将本地容器存储中的镜像推送到远端仓库
SRC=containers-storage:helianthus:latest \
DST=docker://registry.example.com/helianthus:latest \
./Containers/skopeo_copy.sh
```

## 环境变量
- `HELIANTHUS_MODE`: 运行模式，`example` 或 `service`（预留）
- `HELIANTHUS_PORT`: 服务监听端口，默认 `8080`

## 说明
- 运行阶段镜像选择了通用的基础镜像，若有更严格的最小化需求，可替换为 distroless。
- 健康检查脚本会对指定端口进行 TCP 探测；若后续提供 HTTP 指标或健康端点，可在脚本中扩展。


