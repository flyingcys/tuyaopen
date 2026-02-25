#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

require_file() {
  local f="$1"
  if [[ ! -f "${f}" ]]; then
    echo "FAIL: missing file ${f}" >&2
    exit 1
  fi
}

require_grep() {
  local pattern="$1"
  local f="$2"
  if ! grep -q "${pattern}" "${f}"; then
    echo "FAIL: pattern '${pattern}' missing in ${f}" >&2
    exit 1
  fi
}

require_file "${APP_DIR}/cron/cron_service.c"
require_file "${APP_DIR}/cron/cron_service.h"
require_file "${APP_DIR}/heartbeat/heartbeat.c"
require_file "${APP_DIR}/heartbeat/heartbeat.h"
require_file "${APP_DIR}/skills/skill_loader.c"
require_file "${APP_DIR}/skills/skill_loader.h"
require_file "${APP_DIR}/tools/tool_cron.c"
require_file "${APP_DIR}/tools/tool_cron.h"

require_grep "cron/cron_service.c" "${APP_DIR}/CMakeLists.txt"
require_grep "heartbeat/heartbeat.c" "${APP_DIR}/CMakeLists.txt"
require_grep "skills/skill_loader.c" "${APP_DIR}/CMakeLists.txt"
require_grep "tools/tool_cron.c" "${APP_DIR}/CMakeLists.txt"

require_grep "skill_loader_init" "${APP_DIR}/mimi.c"
require_grep "cron_service_init" "${APP_DIR}/mimi.c"
require_grep "heartbeat_init" "${APP_DIR}/mimi.c"
require_grep "cron_service_start" "${APP_DIR}/mimi.c"
require_grep "heartbeat_start" "${APP_DIR}/mimi.c"

require_grep "tools/tool_cron.h" "${APP_DIR}/tools/tool_registry.c"
require_grep "cron_add" "${APP_DIR}/tools/tool_registry.c"
require_grep "cron_list" "${APP_DIR}/tools/tool_registry.c"
require_grep "cron_remove" "${APP_DIR}/tools/tool_registry.c"

require_grep "cron_add" "${APP_DIR}/agent/context_builder.c"
require_grep "skill_loader_build_summary" "${APP_DIR}/agent/context_builder.c"
require_grep "patch_tool_input_with_context" "${APP_DIR}/agent/agent_loop.c"

echo "PASS: cron/heartbeat/skills/tool_cron port and integration guards are present."
