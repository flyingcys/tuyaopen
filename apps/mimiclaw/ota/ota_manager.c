#include "ota_manager.h"

static const char *TAG = "ota";

OPERATE_RET ota_update_from_url(const char *url)
{
    if (!url || url[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    MIMI_LOGW(TAG, "ota_update_from_url stub: %s", url);
    return OPRT_NOT_SUPPORTED;
}
