# mbedTLS 在 RTOS 单片机上的移植宏配置分析

## 背景

mbedTLS 默认配置更偏向 Linux、Windows、BSD、macOS 等有完整 C/POSIX 运行环境的平台。典型默认能力包括：

- BSD socket 网络 API
- 文件系统 API
- `time()` / `gmtime()` 等时间 API
- 标准 C 库的 `calloc/free`
- pthread 或可替代线程锁

RTOS 单片机通常不具备完整 POSIX 环境，因此移植 mbedTLS 时，经常需要禁用这些宏：

```c
#undef MBEDTLS_NET_C
#undef MBEDTLS_FS_IO
#undef MBEDTLS_HAVE_TIME_DATE
#undef MBEDTLS_HAVE_TIME
```

禁用这些宏不代表 TLS 功能不能用，而是表示“不使用 mbedTLS 自带的 POSIX/文件/时间默认适配”，由应用或平台层提供替代实现。

## 四个宏的作用

### MBEDTLS_NET_C

`MBEDTLS_NET_C` 启用 mbedTLS 自带的 TCP/UDP 网络封装模块：

- 头文件：`include/mbedtls/net_sockets.h`
- 实现文件：`library/net_sockets.c`

启用后会提供：

- `mbedtls_net_init()`
- `mbedtls_net_connect()`
- `mbedtls_net_bind()`
- `mbedtls_net_accept()`
- `mbedtls_net_send()`
- `mbedtls_net_recv()`
- `mbedtls_net_recv_timeout()`
- `mbedtls_net_set_block()`
- `mbedtls_net_set_nonblock()`
- `mbedtls_net_poll()`
- `mbedtls_net_free()`

这些函数内部依赖 Linux/Windows 风格接口，例如：

- `socket()`
- `connect()`
- `bind()`
- `listen()`
- `accept()`
- `send()` / `recv()`
- `read()` / `write()`
- `select()`
- `fcntl()`
- `close()` / `shutdown()`
- `getaddrinfo()` / `freeaddrinfo()`
- `errno`

mbedTLS 源码中也明确说明：该模块只适合 POSIX/Unix 和 Windows；其他平台应禁用它，并通过 `mbedtls_ssl_set_bio()` 传入自己的网络回调。

RTOS 单片机通常应该禁用：

```c
#undef MBEDTLS_NET_C
```

然后自己实现：

```c
static int tls_send(void *ctx, const unsigned char *buf, size_t len);
static int tls_recv(void *ctx, unsigned char *buf, size_t len);
static int tls_recv_timeout(void *ctx, unsigned char *buf, size_t len, uint32_t timeout);

mbedtls_ssl_set_bio(&ssl, net_ctx, tls_send, tls_recv, tls_recv_timeout);
```

如果底层网络栈是 lwIP，并且平台已经提供 POSIX socket 适配，也可以保留 `MBEDTLS_NET_C` 或提供类似 ESP-IDF 的 `net_sockets.c` port。但如果是裸 lwIP raw API、厂商私有 TCP API、蜂窝模组 AT socket、TLS over BLE 等场景，就应该走自定义 BIO。

### MBEDTLS_FS_IO

`MBEDTLS_FS_IO` 启用 mbedTLS 中依赖文件系统的函数。

启用后会编译或声明这类 API：

- 从文件读取证书：
  - `mbedtls_x509_crt_parse_file()`
  - `mbedtls_x509_crt_parse_path()`
- 从文件读取私钥：
  - `mbedtls_pk_parse_keyfile()`
  - `mbedtls_pk_parse_public_keyfile()`
- 从文件读取 CRL/CSR：
  - `mbedtls_x509_crl_parse_file()`
  - `mbedtls_x509_csr_parse_file()`
- 文件 hash：
  - `mbedtls_md_file()`
- DRBG seed 文件读写：
  - `mbedtls_ctr_drbg_write_seed_file()`
  - `mbedtls_ctr_drbg_update_seed_file()`
  - `mbedtls_hmac_drbg_write_seed_file()`
  - `mbedtls_hmac_drbg_update_seed_file()`
- MPI 文件读写调试接口等。

这些函数内部依赖：

- `fopen()`
- `fread()`
- `fwrite()`
- `fclose()`
- `fprintf()`
- 标准文件路径和目录遍历能力。

RTOS 单片机通常没有完整文件系统，应该禁用：

```c
#undef MBEDTLS_FS_IO
```

禁用后仍然可以使用“从内存解析”的 API：

```c
mbedtls_x509_crt_parse(&cacert, ca_pem, ca_pem_len);
mbedtls_pk_parse_key(&pkey, key_pem, key_pem_len, NULL, 0, f_rng, p_rng);
mbedtls_x509_crl_parse(&crl, crl_buf, crl_len);
```

证书、私钥、PSK 可以来自：

- 编译进固件的 const 数组
- Flash 分区
- NVS/KV
- 安全存储
- OTA 下载后的内存 buffer
- efuse/安全硬件句柄

如果产品确实有 LittleFS/FATFS，可以选择保留 `MBEDTLS_FS_IO`，但需要确保标准 C 文件 API 已被正确适配。很多单片机项目即使有文件系统，也更建议在业务层读文件到内存，再调用 mbedTLS 的内存解析 API，以减少 mbedTLS 对文件系统的耦合。

### MBEDTLS_HAVE_TIME

`MBEDTLS_HAVE_TIME` 表示系统有 `time.h` 和 `time()`。

它不要求系统时间一定是正确日历时间，只要求有基本时间函数可用，主要用于相对时间或会话生命周期管理。

启用后会影响：

- TLS session ticket 生命周期
- TLS session cache 过期
- DTLS cookie 超时
- 部分 TLS 1.3 ticket age 计算
- mbedTLS 平台时间抽象

ESP-IDF 文档中的解释也很清楚：这个选项不要求系统时间正确，但会启用需要相对时间的功能，例如 session ticket 或 session cache 的周期性过期。

RTOS 上如果没有 `time()`，可以禁用：

```c
#undef MBEDTLS_HAVE_TIME
```

禁用后的影响：

- session cache/ticket 的过期管理能力受限或不可用。
- 某些依赖时间的 TLS server 功能会降级。
- 不影响最基础的 TLS client 握手和收发。
- 如果同时启用某些需要时间的模块，`check_config.h` 可能报错。

如果 RTOS 有系统 tick，但没有 POSIX `time()`，有两种做法：

1. 禁用 `MBEDTLS_HAVE_TIME`
   - 最简单。
   - 适合只做 TLS client，且不需要 session ticket/cache 过期管理的设备。

2. 启用 `MBEDTLS_HAVE_TIME` 并提供替代时间函数
   - 定义 `MBEDTLS_PLATFORM_TIME_ALT`，运行时用 `mbedtls_platform_set_time()` 注册。
   - 或定义 `MBEDTLS_PLATFORM_TIME_MACRO` / `MBEDTLS_PLATFORM_TIME_TYPE_MACRO`。
   - 适合需要 session ticket/cache 或证书时间校验的设备。

示例：

```c
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_TIME_ALT
```

初始化时：

```c
static mbedtls_time_t rtos_time(mbedtls_time_t *t)
{
    mbedtls_time_t now = platform_get_unix_time_or_uptime();
    if (t != NULL) {
        *t = now;
    }
    return now;
}

mbedtls_platform_set_time(rtos_time);
```

如果返回的是 uptime，而不是 Unix epoch 时间，则不能同时启用 `MBEDTLS_HAVE_TIME_DATE` 做证书有效期校验。

### MBEDTLS_HAVE_TIME_DATE

`MBEDTLS_HAVE_TIME_DATE` 表示系统不仅有 `time()`，而且系统日期时间是正确的，并且有可用的 `gmtime()`/`gmtime_r()` 能力。

它主要用于 X.509 证书有效期校验：

- `notBefore`
- `notAfter`

启用后，证书验证会比较当前时间和证书有效期。如果设备时间不正确，可能返回：

- `MBEDTLS_X509_BADCERT_FUTURE`
- `MBEDTLS_X509_BADCERT_EXPIRED`

该宏依赖 `MBEDTLS_HAVE_TIME`。mbedTLS 的 `check_config.h` 会检查：

```c
#if defined(MBEDTLS_HAVE_TIME_DATE) && !defined(MBEDTLS_HAVE_TIME)
#error "MBEDTLS_HAVE_TIME_DATE without MBEDTLS_HAVE_TIME does not make sense"
#endif
```

RTOS 单片机上通常建议禁用：

```c
#undef MBEDTLS_HAVE_TIME_DATE
```

原因是很多设备启动时没有 RTC、没有电池备份时间，也不能在 TLS 连接前保证 SNTP 已同步。一旦时间错误，TLS 连接会因为证书“未生效”或“已过期”失败。

禁用后的影响：

- mbedTLS 不检查证书有效期。
- 仍然可以检查证书链签名、CA、主机名等。
- 安全性会降低，因为过期证书不会被 mbedTLS 拒绝。

如果产品必须做完整证书校验，应该：

1. 启用 `MBEDTLS_HAVE_TIME`。
2. 启用 `MBEDTLS_HAVE_TIME_DATE`。
3. 在 TLS 连接前完成可信时间同步。
4. 提供 `gmtime_r()` 或 `MBEDTLS_PLATFORM_GMTIME_R_ALT`。

可信时间来源可以是：

- SNTP/NTP
- 蜂窝网络时间
- 网关下发时间
- 安全 RTC
- 出厂写入并持续维护的 RTC

如果设备必须先通过 TLS 才能获取时间，可以考虑：

- 首次连接禁用时间校验，但启用 CA 和 hostname 校验。
- 连接后同步可信时间。
- 后续连接启用证书有效期校验。
- 或使用短链路到固定可信服务器，配合证书 pinning。

## 四个宏的推荐配置

### 普通 RTOS TLS Client

适合 Wi-Fi/以太网/蜂窝 IoT 设备，证书和私钥从内存提供：

```c
#undef MBEDTLS_NET_C
#undef MBEDTLS_FS_IO
#undef MBEDTLS_HAVE_TIME_DATE
#undef MBEDTLS_HAVE_TIME
```

需要自己提供：

- BIO 网络回调
- 随机数熵源
- 内存分配
- 可选线程锁
- 证书内存加载

### RTOS TLS Client + 有可信时间

适合设备能在 TLS 前完成 SNTP/RTC 同步：

```c
#undef MBEDTLS_NET_C
#undef MBEDTLS_FS_IO
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME_DATE
```

需要提供：

- `time()`
- `gmtime_r()` 或 `MBEDTLS_PLATFORM_GMTIME_R_ALT`
- 或通过 `MBEDTLS_PLATFORM_TIME_ALT` 注册时间函数。

### RTOS + lwIP POSIX socket 适配完整

如果平台像 ESP-IDF 一样已经把 lwIP socket 适配成 POSIX/BSD 风格接口，则可以选择保留或移植 `MBEDTLS_NET_C`。

但更稳妥的方式仍是：

```c
#undef MBEDTLS_NET_C
```

由平台 TLS 封装层自己控制 socket 创建、超时、非阻塞和错误转换，再通过 `mbedtls_ssl_set_bio()` 交给 mbedTLS。

### RTOS + 文件系统

如果已经有 FATFS/LittleFS，并且标准 `fopen/fread/fclose` 可用：

```c
#define MBEDTLS_FS_IO
```

否则建议禁用，并使用内存解析 API：

```c
#undef MBEDTLS_FS_IO
```

## RTOS 移植必须补齐的接口

只禁用四个宏还不够。完整移植 mbedTLS 到 RTOS 单片机，通常还需要处理以下平台能力。

### 1. 网络 BIO

最核心接口是：

```c
int f_send(void *ctx, const unsigned char *buf, size_t len);
int f_recv(void *ctx, unsigned char *buf, size_t len);
int f_recv_timeout(void *ctx, unsigned char *buf, size_t len, uint32_t timeout);
```

返回值要符合 mbedTLS 约定：

- 成功：返回实际收发字节数。
- 非阻塞暂不可读：返回 `MBEDTLS_ERR_SSL_WANT_READ`。
- 非阻塞暂不可写：返回 `MBEDTLS_ERR_SSL_WANT_WRITE`。
- 超时：返回 `MBEDTLS_ERR_SSL_TIMEOUT`。
- 连接断开：返回 `MBEDTLS_ERR_NET_CONN_RESET` 或业务自定义错误。
- 其他错误：返回负数错误码。

不要直接把底层网络栈的 errno 或厂商错误码原样返回给 mbedTLS。

### 2. 随机数和熵源

TLS 必须有可靠随机数。通常至少需要：

```c
mbedtls_entropy_init(&entropy);
mbedtls_ctr_drbg_init(&ctr_drbg);
mbedtls_ctr_drbg_seed(&ctr_drbg,
                      mbedtls_entropy_func,
                      &entropy,
                      personalization,
                      personalization_len);
```

RTOS 平台应提供硬件随机源：

```c
#define MBEDTLS_ENTROPY_HARDWARE_ALT

int mbedtls_hardware_poll(void *data,
                          unsigned char *output,
                          size_t len,
                          size_t *olen)
{
    platform_trng_get(output, len);
    *olen = len;
    return 0;
}
```

如果没有 TRNG，不能简单使用 tick、MAC 地址、ADC 噪声拼凑为强随机源。应使用芯片安全模块、无线 PHY 随机源、TRNG 外设，或设计 NV seed 机制并评估安全性。

### 3. 内存分配

mbedTLS 默认使用 `calloc/free`。RTOS 常需要接到堆、PSRAM 或静态内存池。

推荐启用：

```c
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
```

然后运行时注册：

```c
mbedtls_platform_set_calloc_free(platform_calloc, platform_free);
```

TuyaOpen 的 `tuya_tls_init()` 就是这样把 mbedTLS 内存适配到 `tal_calloc/tal_free` 或 PSRAM 分配器。

### 4. 线程锁

如果多个任务可能同时使用 mbedTLS，或启用了需要全局锁的模块，应启用：

```c
#define MBEDTLS_THREADING_C
#define MBEDTLS_THREADING_ALT
```

并注册：

```c
mbedtls_threading_set_alt(mutex_init,
                          mutex_free,
                          mutex_lock,
                          mutex_unlock);
```

这些函数应映射到 RTOS mutex，例如 FreeRTOS semaphore/mutex。

如果系统单线程使用 mbedTLS，且不会跨任务共享 context，可以不启用 threading，但要保证上层不会并发调用同一个 `mbedtls_ssl_context`。

### 5. 时间和定时器

如果只做 TLS client，并且禁用证书有效期校验，可以不提供 wall-clock。

但以下场景需要时间能力：

- 证书有效期校验：需要正确 Unix 时间和 `gmtime_r()`。
- session ticket/cache：需要相对时间。
- DTLS retransmission：通常需要 `mbedtls_ssl_set_timer_cb()` 和 timer 回调。

DTLS 定时器示例：

```c
mbedtls_ssl_set_timer_cb(&ssl, timer_ctx, timer_set_delay, timer_get_delay);
```

### 6. 证书和私钥加载

禁用 `MBEDTLS_FS_IO` 后，不能使用 `*_parse_file()`。应改为：

```c
mbedtls_x509_crt_parse(&ca, ca_buf, ca_len);
mbedtls_x509_crt_parse(&client_cert, cert_buf, cert_len);
mbedtls_pk_parse_key(&client_key, key_buf, key_len, password, password_len, f_rng, p_rng);
```

PEM 格式通常要求 buffer 包含结尾 `\0`，长度包含该 NUL。DER 格式不需要。

### 7. 调试输出

如果启用 mbedTLS debug，可注册：

```c
mbedtls_ssl_conf_dbg(&conf, debug_cb, NULL);
```

映射到 RTOS log 系统。生产版本通常关闭 `MBEDTLS_DEBUG_C` 以减少体积。

## 最小 TLS Client 移植流程

RTOS 单片机上一个典型 TLS client 流程如下：

1. 配置裁剪
   - 禁用 `MBEDTLS_NET_C`
   - 禁用 `MBEDTLS_FS_IO`
   - 默认禁用 `MBEDTLS_HAVE_TIME_DATE`
   - 根据是否需要 session 过期管理决定是否启用 `MBEDTLS_HAVE_TIME`

2. 平台初始化
   - 注册 `calloc/free`
   - 注册 threading mutex
   - 初始化 entropy/CTR_DRBG
   - 初始化网络栈

3. 创建 TCP 连接
   - 由平台网络层完成 DNS、socket、connect、timeout。
   - 得到 socket fd 或厂商连接句柄。

4. 配置 mbedTLS
   - `mbedtls_ssl_init()`
   - `mbedtls_ssl_config_init()`
   - `mbedtls_ssl_config_defaults()`
   - `mbedtls_ssl_conf_rng()`
   - `mbedtls_ssl_conf_ca_chain()`
   - `mbedtls_ssl_set_hostname()`
   - `mbedtls_ssl_setup()`

5. 绑定 BIO
   - `mbedtls_ssl_set_bio(&ssl, net_ctx, f_send, f_recv, f_recv_timeout)`

6. 握手
   - 循环调用 `mbedtls_ssl_handshake()`
   - 正确处理 `MBEDTLS_ERR_SSL_WANT_READ` 和 `MBEDTLS_ERR_SSL_WANT_WRITE`

7. 收发
   - `mbedtls_ssl_read()`
   - `mbedtls_ssl_write()`

8. 释放
   - `mbedtls_ssl_close_notify()`
   - `mbedtls_ssl_free()`
   - `mbedtls_ssl_config_free()`
   - `mbedtls_x509_crt_free()`
   - `mbedtls_ctr_drbg_free()`
   - `mbedtls_entropy_free()`
   - 关闭底层 TCP 连接

## 与 ESP-IDF 和 TuyaOpen 的对照

ESP-IDF 的做法：

- `MBEDTLS_NET_C` 在 `esp_config.h` 中被 `#undef`。
- ESP-IDF 移除了上游 `library/net_sockets.c`，改用自己的 `components/mbedtls/port/net_sockets.c`。
- lwIP + VFS 提供 `read/write/select/fcntl/socket` 等接口。
- `MBEDTLS_FS_IO` 受 Kconfig 控制，依赖 VFS。
- `MBEDTLS_HAVE_TIME_DATE` 默认关闭，避免无正确时间导致证书验证失败。
- 硬件随机源通过 `MBEDTLS_ENTROPY_HARDWARE_ALT` 接到 `esp_fill_random()`。

TuyaOpen-xiaozhi 的做法：

- `tuya_tls_config.h` 通过 `ENABLE_MBEDTLS_NET_C`、`ENABLE_MBEDTLS_FS_IO`、`ENABLE_MBEDTLS_HAVE_TIME`、`ENABLE_MBEDTLS_HAVE_TIME_DATE` 控制这些宏。
- 默认倾向禁用 `MBEDTLS_NET_C`，因为 Tuya TLS 自己通过 `mbedtls_ssl_set_bio()` 绑定 `tal_net_send()` / `tal_net_recv()`。
- `tuya_tls_init()` 注册 mutex、calloc/free、entropy/CTR_DRBG。
- 证书解析使用内存 API，而不是依赖文件路径。

## 推荐移植策略

RTOS 单片机推荐策略：

1. 禁用 `MBEDTLS_NET_C`
   - 网络由平台层负责，mbedTLS 只做 TLS record 和 handshake。

2. 禁用 `MBEDTLS_FS_IO`
   - 证书、私钥、PSK 由内存 buffer、安全存储或 flash 分区提供。

3. 默认禁用 `MBEDTLS_HAVE_TIME_DATE`
   - 除非能保证 TLS 前已有可信时间。

4. 根据功能决定是否启用 `MBEDTLS_HAVE_TIME`
   - 只做基础 TLS client 可禁用。
   - 需要 session ticket/cache 或证书时间校验时启用。

5. 必须提供可靠熵源
   - TLS 安全性强依赖随机数，不能忽略。

6. 明确错误码转换
   - 网络回调必须把底层错误转换成 mbedTLS 期望的 `WANT_READ/WANT_WRITE/TIMEOUT/CONN_RESET`。

7. 优先使用内存 API
   - 不要为了 mbedTLS 保留文件系统依赖。

8. 如果有硬件加速，走 mbedTLS `ALT` port
   - 例如 ESP-IDF 的 AES/SHA/MPI/ECC 硬件加速就是通过 `*_ALT` 宏接入。

## 常见问题

### 禁用 MBEDTLS_NET_C 后还能联网吗？

可以。禁用的是 mbedTLS 自带的 socket helper，不是 TLS 网络能力。只要通过 `mbedtls_ssl_set_bio()` 提供收发回调即可。

### 禁用 MBEDTLS_FS_IO 后还能加载证书吗？

可以。用 `mbedtls_x509_crt_parse()` 从内存加载证书即可。

### 禁用 MBEDTLS_HAVE_TIME_DATE 后证书还安全吗？

证书链签名、CA、hostname 仍可验证，但证书有效期不会检查。安全性低于完整校验。对于无可靠 RTC 的 IoT 设备，这是常见取舍。

### MBEDTLS_HAVE_TIME 和 MBEDTLS_HAVE_TIME_DATE 有什么区别？

`MBEDTLS_HAVE_TIME` 表示有时间函数，可用于相对时间。`MBEDTLS_HAVE_TIME_DATE` 表示当前日期可信，可用于证书有效期校验。后者依赖前者。

### RTOS 上最容易漏掉什么？

最容易漏掉三件事：

- 熵源不可靠。
- BIO 回调错误码转换不正确。
- 启用证书时间校验但系统时间未同步。

