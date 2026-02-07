#!/usr/bin/env bash
set -euo pipefail

HOST="${METALV_MCP_HOST:-127.0.0.1}"
PORT="${METALV_MCP_PORT:-5566}"

exec socat - TCP:"${HOST}":"${PORT}"
