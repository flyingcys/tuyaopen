#include "memory_store.h"
#include "mimi_config.h"

#include "tal_fs.h"

#include <time.h>

static const char *TAG = "memory";

static void get_date_str(char *buf, size_t size, int days_ago)
{
    time_t now = time(NULL) - (time_t)days_ago * 86400;
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    strftime(buf, size, "%Y-%m-%d", &tm_now);
}

OPERATE_RET memory_store_init(void)
{
    MIMI_LOGI(TAG, "memory store init: %s", MIMI_SPIFFS_MEMORY_DIR);
    return OPRT_OK;
}

OPERATE_RET memory_read_long_term(char *buf, size_t size)
{
    if (!buf || size == 0) {
        return OPRT_INVALID_PARM;
    }

    TUYA_FILE f = tal_fopen(MIMI_MEMORY_FILE, "r");
    if (!f) {
        buf[0] = '\0';
        return OPRT_NOT_FOUND;
    }

    int n = tal_fread(buf, (int)(size - 1), f);
    if (n < 0) {
        buf[0] = '\0';
        tal_fclose(f);
        return OPRT_COM_ERROR;
    }
    buf[n] = '\0';
    tal_fclose(f);
    return OPRT_OK;
}

OPERATE_RET memory_write_long_term(const char *content)
{
    if (!content) {
        return OPRT_INVALID_PARM;
    }

    TUYA_FILE f = tal_fopen(MIMI_MEMORY_FILE, "w");
    if (!f) {
        return OPRT_FILE_OPEN_FAILED;
    }

    int n = tal_fwrite((void *)content, (int)strlen(content), f);
    tal_fclose(f);
    if (n < 0) {
        return OPRT_COM_ERROR;
    }
    return OPRT_OK;
}

OPERATE_RET memory_append_today(const char *note)
{
    if (!note) {
        return OPRT_INVALID_PARM;
    }

    char date_str[16] = {0};
    char path[128] = {0};
    get_date_str(date_str, sizeof(date_str), 0);
    snprintf(path, sizeof(path), "%s/%s.md", MIMI_SPIFFS_MEMORY_DIR, date_str);

    TUYA_FILE f = tal_fopen(path, "a");
    if (!f) {
        f = tal_fopen(path, "w");
        if (!f) {
            return OPRT_FILE_OPEN_FAILED;
        }
        char hdr[64] = {0};
        int hn = snprintf(hdr, sizeof(hdr), "# %s\n\n", date_str);
        if (hn > 0) {
            (void)tal_fwrite(hdr, hn, f);
        }
    }

    (void)tal_fwrite((void *)note, (int)strlen(note), f);
    (void)tal_fwrite("\n", 1, f);
    tal_fclose(f);
    return OPRT_OK;
}

OPERATE_RET memory_read_recent(char *buf, size_t size, int days)
{
    if (!buf || size == 0 || days <= 0) {
        return OPRT_INVALID_PARM;
    }

    size_t off = 0;
    buf[0] = '\0';

    for (int i = 0; i < days && off < size - 1; i++) {
        char date_str[16] = {0};
        char path[128] = {0};

        get_date_str(date_str, sizeof(date_str), i);
        snprintf(path, sizeof(path), "%s/%s.md", MIMI_SPIFFS_MEMORY_DIR, date_str);

        TUYA_FILE f = tal_fopen(path, "r");
        if (!f) {
            continue;
        }

        if (off > 0 && off < size - 8) {
            off += snprintf(buf + off, size - off, "\n---\n");
        }

        int n = tal_fread(buf + off, (int)(size - off - 1), f);
        if (n > 0) {
            off += (size_t)n;
        }
        buf[off] = '\0';
        tal_fclose(f);
    }

    return OPRT_OK;
}
