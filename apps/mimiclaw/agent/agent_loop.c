#include "agent_loop.h"

#include "agent/context_builder.h"
#include "bus/message_bus.h"
#include "llm/llm_proxy.h"
#include "memory/session_mgr.h"
#include "mimi_config.h"
#include "tools/tool_registry.h"

#include "cJSON.h"

static const char *TAG = "agent";
static THREAD_HANDLE s_agent_thread = NULL;

#define TOOL_OUTPUT_SIZE (8 * 1024)

static cJSON *build_assistant_content(const llm_response_t *resp)
{
    cJSON *content = cJSON_CreateArray();
    if (!content || !resp) {
        return content;
    }

    if (resp->text && resp->text_len > 0) {
        cJSON *text_block = cJSON_CreateObject();
        if (text_block) {
            cJSON_AddStringToObject(text_block, "type", "text");
            cJSON_AddStringToObject(text_block, "text", resp->text);
            cJSON_AddItemToArray(content, text_block);
        }
    }

    for (int i = 0; i < resp->call_count; i++) {
        const llm_tool_call_t *call = &resp->calls[i];
        cJSON *tool_block = cJSON_CreateObject();
        if (!tool_block) {
            continue;
        }

        cJSON_AddStringToObject(tool_block, "type", "tool_use");
        cJSON_AddStringToObject(tool_block, "id", call->id);
        cJSON_AddStringToObject(tool_block, "name", call->name);

        cJSON *input = cJSON_Parse(call->input);
        if (!input) {
            input = cJSON_CreateObject();
        }
        cJSON_AddItemToObject(tool_block, "input", input);
        cJSON_AddItemToArray(content, tool_block);
    }

    return content;
}

static cJSON *build_tool_results(const llm_response_t *resp, char *tool_output, size_t tool_output_size)
{
    cJSON *content = cJSON_CreateArray();
    if (!content || !resp || !tool_output || tool_output_size == 0) {
        return content;
    }

    for (int i = 0; i < resp->call_count; i++) {
        const llm_tool_call_t *call = &resp->calls[i];
        tool_output[0] = '\0';

        OPERATE_RET rt = tool_registry_execute(call->name, call->input, tool_output, tool_output_size);
        if (rt != OPRT_OK && tool_output[0] == '\0') {
            snprintf(tool_output, tool_output_size, "Tool execute failed: %d", rt);
        }

        MIMI_LOGI(TAG, "tool=%s result_bytes=%u", call->name, (unsigned)strlen(tool_output));

        cJSON *result_block = cJSON_CreateObject();
        if (!result_block) {
            continue;
        }

        cJSON_AddStringToObject(result_block, "type", "tool_result");
        cJSON_AddStringToObject(result_block, "tool_use_id", call->id);
        cJSON_AddStringToObject(result_block, "content", tool_output);
        cJSON_AddItemToArray(content, result_block);
    }

    return content;
}

static void agent_loop_task(void *arg)
{
    (void)arg;

    const char *working_phrases[] = {"mimi is working...", "mimi is thinking...", "mimi is pondering...",
                                     "mimi is on it...", "mimi is cooking..."};
    const int phrase_count = (int)(sizeof(working_phrases) / sizeof(working_phrases[0]));

    char *system_prompt = calloc(1, MIMI_CONTEXT_BUF_SIZE);
    char *history_json = calloc(1, MIMI_LLM_STREAM_BUF_SIZE);
    char *tool_output = calloc(1, TOOL_OUTPUT_SIZE);

    MIMI_LOGI(TAG, "agent loop started");
    if (!system_prompt || !history_json || !tool_output) {
        MIMI_LOGE(TAG, "alloc buffers failed");
        free(system_prompt);
        free(history_json);
        free(tool_output);
        return;
    }

    const char *tools_json = tool_registry_get_tools_json();

    while (1) {
        mimi_msg_t in_msg = {0};
        if (message_bus_pop_inbound(&in_msg, MIMI_WAIT_FOREVER) != OPRT_OK) {
            continue;
        }

        MIMI_LOGI(TAG, "processing msg %s:%s", in_msg.channel, in_msg.chat_id);

        if (context_build_system_prompt(system_prompt, MIMI_CONTEXT_BUF_SIZE) != OPRT_OK) {
            free(in_msg.content);
            continue;
        }

        if (session_get_history_json(in_msg.chat_id, history_json, MIMI_LLM_STREAM_BUF_SIZE, MIMI_AGENT_MAX_HISTORY) !=
            OPRT_OK) {
            snprintf(history_json, MIMI_LLM_STREAM_BUF_SIZE, "[]");
        }

        cJSON *messages = cJSON_Parse(history_json);
        if (!messages || !cJSON_IsArray(messages)) {
            cJSON_Delete(messages);
            messages = cJSON_CreateArray();
        }

        const char *user_text = in_msg.content ? in_msg.content : "";
        cJSON *user_msg = cJSON_CreateObject();
        cJSON_AddStringToObject(user_msg, "role", "user");
        cJSON_AddStringToObject(user_msg, "content", user_text);
        cJSON_AddItemToArray(messages, user_msg);

        char *final_text = NULL;
        int iteration = 0;

        while (iteration < MIMI_AGENT_MAX_TOOL_ITER) {
            mimi_msg_t status = {0};
            strncpy(status.channel, in_msg.channel, sizeof(status.channel) - 1);
            strncpy(status.chat_id, in_msg.chat_id, sizeof(status.chat_id) - 1);
            status.content = strdup(working_phrases[tal_system_get_random(phrase_count)]);
            if (status.content) {
                if (message_bus_push_outbound(&status) != OPRT_OK) {
                    free(status.content);
                }
            }

            llm_response_t resp;
            OPERATE_RET rt = llm_chat_tools(system_prompt, messages, tools_json, &resp);
            if (rt != OPRT_OK) {
                MIMI_LOGE(TAG, "llm_chat_tools failed: %d", rt);
                break;
            }

            if (!resp.tool_use) {
                if (resp.text && resp.text[0] != '\0') {
                    final_text = strdup(resp.text);
                }
                llm_response_free(&resp);
                break;
            }

            MIMI_LOGI(TAG, "tool iteration=%d call_count=%d", iteration + 1, resp.call_count);

            cJSON *asst_msg = cJSON_CreateObject();
            cJSON_AddStringToObject(asst_msg, "role", "assistant");
            cJSON_AddItemToObject(asst_msg, "content", build_assistant_content(&resp));
            cJSON_AddItemToArray(messages, asst_msg);

            cJSON *result_msg = cJSON_CreateObject();
            cJSON_AddStringToObject(result_msg, "role", "user");
            cJSON_AddItemToObject(result_msg, "content", build_tool_results(&resp, tool_output, TOOL_OUTPUT_SIZE));
            cJSON_AddItemToArray(messages, result_msg);

            llm_response_free(&resp);
            iteration++;
        }

        cJSON_Delete(messages);

        if (final_text && final_text[0] != '\0') {
            (void)session_append(in_msg.chat_id, "user", user_text);
            (void)session_append(in_msg.chat_id, "assistant", final_text);

            mimi_msg_t out_msg = {0};
            strncpy(out_msg.channel, in_msg.channel, sizeof(out_msg.channel) - 1);
            strncpy(out_msg.chat_id, in_msg.chat_id, sizeof(out_msg.chat_id) - 1);
            out_msg.content = final_text;
            if (message_bus_push_outbound(&out_msg) != OPRT_OK) {
                free(out_msg.content);
            } else {
                final_text = NULL;
            }
        } else {
            mimi_msg_t out_msg = {0};
            strncpy(out_msg.channel, in_msg.channel, sizeof(out_msg.channel) - 1);
            strncpy(out_msg.chat_id, in_msg.chat_id, sizeof(out_msg.chat_id) - 1);
            out_msg.content = strdup("Sorry, I encountered an error.");
            if (out_msg.content) {
                if (message_bus_push_outbound(&out_msg) != OPRT_OK) {
                    free(out_msg.content);
                }
            }
        }

        free(final_text);
        free(in_msg.content);
        MIMI_LOGI(TAG, "free heap=%d", tal_system_get_free_heap_size());
    }
}

OPERATE_RET agent_loop_init(void)
{
    return OPRT_OK;
}

OPERATE_RET agent_loop_start(void)
{
    if (s_agent_thread) {
        return OPRT_OK;
    }

    THREAD_CFG_T cfg = {0};
    cfg.stackDepth = MIMI_AGENT_STACK;
    cfg.priority = THREAD_PRIO_1;
    cfg.thrdname = "mimi_agent";

    OPERATE_RET rt = tal_thread_create_and_start(&s_agent_thread, NULL, NULL, agent_loop_task, NULL, &cfg);
    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "create agent thread failed: %d", rt);
        return rt;
    }

    return OPRT_OK;
}
