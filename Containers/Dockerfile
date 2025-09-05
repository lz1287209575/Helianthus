# syntax=docker/dockerfile:1.6

############################
# Builder
############################
FROM ubuntu:22.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      ca-certificates curl git build-essential clang bazel && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . /workspace

# 预热子模块与依赖
RUN git submodule update --init --recursive || true

# 构建：全量或示例
RUN bazel build //... || bazel build //Examples:PersistenceMetricsExample

# 复制示例二进制（如存在）
RUN mkdir -p /out/bin && \
    (test -f bazel-bin/Examples/PersistenceMetricsExample && \
     cp bazel-bin/Examples/PersistenceMetricsExample /out/bin/HelianthusExample || true)

############################
# Runtime
############################
FROM ubuntu:22.04 AS runtime

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      ca-certificates tzdata bash netcat-openbsd && \
    rm -rf /var/lib/apt/lists/*

ENV HELIANTHUS_MODE=example \
    HELIANTHUS_PORT=8080 \
    LANG=C.UTF-8 LC_ALL=C.UTF-8

WORKDIR /app
COPY --from=builder /out/bin /app/bin
COPY Containers/entrypoint.sh /app/entrypoint.sh
COPY Containers/healthcheck.sh /app/healthcheck.sh

RUN chmod +x /app/entrypoint.sh /app/healthcheck.sh

HEALTHCHECK --interval=15s --timeout=5s --retries=3 CMD /app/healthcheck.sh || exit 1

EXPOSE 8080
ENTRYPOINT ["/app/entrypoint.sh"]





