#ifndef TUYA_CLOUD_SERVICE_TEST_HOOKS_H_
#define TUYA_CLOUD_SERVICE_TEST_HOOKS_H_

#include "tuya_endpoint.h"

typedef struct {
    int              calls;
    const char      *last_region;
    const char      *last_env;
    tuya_endpoint_t *last_endpoint;
    int              return_value;
} tuya_endpoint_iotdns_fake_t;

extern tuya_endpoint_iotdns_fake_t g_tuya_endpoint_iotdns_fake;

void tuya_endpoint_iotdns_fake_reset(void);

#endif /* TUYA_CLOUD_SERVICE_TEST_HOOKS_H_ */
