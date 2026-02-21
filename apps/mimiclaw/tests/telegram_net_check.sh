#!/usr/bin/env bash
set -euo pipefail

TARGET_HOST="api.telegram.org"
TARGET_PORT="443"
MAX_TIME="8"

ok=0
fail=0

print_line() {
  printf '%s\n' "------------------------------------------------------------"
}

pass() {
  printf '[PASS] %s\n' "$1"
  ok=$((ok + 1))
}

warn() {
  printf '[FAIL] %s\n' "$1"
  fail=$((fail + 1))
}

run_quiet() {
  local name="$1"
  shift
  if "$@" >/tmp/tg_net_check.out 2>/tmp/tg_net_check.err; then
    pass "${name}"
    return 0
  fi
  warn "${name}"
  printf '  stderr: %s\n' "$(head -n 2 /tmp/tg_net_check.err | tr '\n' ' ')" || true
  return 1
}

print_line
echo "Telegram 网络诊断"
echo "时间: $(date '+%F %T %Z')"
echo "目标: ${TARGET_HOST}:${TARGET_PORT}"
echo "当前代理环境:"
echo "  http_proxy=${http_proxy:-<empty>}"
echo "  https_proxy=${https_proxy:-<empty>}"
echo "  all_proxy=${all_proxy:-<empty>}"
print_line

# 1) DNS
if command -v getent >/dev/null 2>&1; then
  run_quiet "DNS 解析 (${TARGET_HOST})" getent hosts "${TARGET_HOST}" || true
else
  run_quiet "DNS 解析 (${TARGET_HOST})" python3 - <<'PY'
import socket
socket.getaddrinfo("api.telegram.org", 443)
print("ok")
PY
fi

# 2) 直连 TCP
run_quiet "直连 TCP 443 (bash /dev/tcp)" timeout "${MAX_TIME}" bash -lc "exec 3<>/dev/tcp/${TARGET_HOST}/${TARGET_PORT}; echo ok >&3; exec 3<&-; exec 3>&-" || true

# 3) 直连 TLS 握手
if command -v openssl >/dev/null 2>&1; then
  run_quiet "直连 TLS 握手 (openssl s_client)" timeout "${MAX_TIME}" bash -lc "openssl s_client -connect ${TARGET_HOST}:${TARGET_PORT} -servername ${TARGET_HOST} -brief < /dev/null | head -n 5" || true
else
  warn "直连 TLS 握手 (openssl s_client) - openssl 不存在"
fi

# 4) 直连 HTTPS（忽略系统代理）
if command -v curl >/dev/null 2>&1; then
  run_quiet "直连 HTTPS HEAD (curl --noproxy '*')" curl --noproxy '*' -I --max-time "${MAX_TIME}" "https://${TARGET_HOST}" || true
else
  warn "直连 HTTPS HEAD - curl 不存在"
fi

# 5) 使用当前 shell 代理变量进行 HTTPS
if command -v curl >/dev/null 2>&1; then
  run_quiet "代理 HTTPS HEAD (curl 使用当前环境变量)" curl -I --max-time "${MAX_TIME}" "https://${TARGET_HOST}" || true
fi

# 6) 可选：Bot API 验证（需传 TG_BOT_TOKEN）
if [[ -n "${TG_BOT_TOKEN:-}" ]] && command -v curl >/dev/null 2>&1; then
  run_quiet "Bot API getMe (直连)" curl --noproxy '*' -sS --max-time "${MAX_TIME}" "https://${TARGET_HOST}/bot${TG_BOT_TOKEN}/getMe" || true
  run_quiet "Bot API getMe (代理环境)" curl -sS --max-time "${MAX_TIME}" "https://${TARGET_HOST}/bot${TG_BOT_TOKEN}/getMe" || true
fi

print_line
echo "汇总: PASS=${ok}, FAIL=${fail}"

if [[ "${fail}" -eq 0 ]]; then
  echo "结论: 当前机器到 Telegram 网络可达。"
  exit 0
fi

echo "结论: 当前机器到 Telegram 存在连通性问题（见 FAIL 项）。"
exit 1
