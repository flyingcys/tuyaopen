#include "tal_log.h"

OPERATE_RET tal_log_print_secure(BOOL_T is_const_fmt, const TAL_LOG_LEVEL_E level, const char *file, const int line,
                                 const char *fmt, ...)
{
    (void)is_const_fmt;
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
    return OPRT_OK;
}

OPERATE_RET tal_log_print_raw(const char *pFmt, ...)
{
    (void)pFmt;
    return OPRT_OK;
}
