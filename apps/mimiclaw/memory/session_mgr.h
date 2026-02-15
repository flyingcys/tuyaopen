#pragma once

#include "mimi_base.h"

OPERATE_RET session_mgr_init(void);
OPERATE_RET session_append(const char *chat_id, const char *role, const char *content);
OPERATE_RET session_get_history_json(const char *chat_id, char *buf, size_t size, int max_msgs);
OPERATE_RET session_clear(const char *chat_id);
void session_list(void);
