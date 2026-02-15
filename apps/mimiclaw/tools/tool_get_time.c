#include "tool_get_time.h"

#include <time.h>

OPERATE_RET tool_get_time_execute(const char *input_json, char *output, size_t output_size)
{
    (void)input_json;

    if (!output || output_size == 0) {
        return OPRT_INVALID_PARM;
    }

    time_t now = time(NULL);
    struct tm local_tm;
    localtime_r(&now, &local_tm);

    strftime(output, output_size, "%Y-%m-%d %H:%M:%S %Z", &local_tm);
    return OPRT_OK;
}
