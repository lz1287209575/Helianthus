#!/usr/bin/env bash
set -euo pipefail

Mode="${HELIANTHUS_MODE:-example}"
Port="${HELIANTHUS_PORT:-8080}"

echo "[ENTRYPOINT] Mode=${Mode} Port=${Port}"

case "${Mode}" in
  example)
    if [ -x "/app/bin/HelianthusExample" ]; then
      exec /app/bin/HelianthusExample
    else
      echo "HelianthusExample binary not found."
      sleep 2
      exit 1
    fi
    ;;
  service)
    echo "Service mode not yet implemented."
    sleep 2
    exit 1
    ;;
  *)
    echo "Unknown HELIANTHUS_MODE=${Mode}"
    exit 1
    ;;
esac





