# ESP-IDF 与 TuyaOpen-xiaozhi 的 mbedTLS 适配分析

## 结论

ESP-IDF 不需要类似 `tuya_tls_init()` 这种由应用显式调用的 TLS 全局初始化，核心原因不是单纯“做了 POSIX 适配”，而是 ESP-IDF 已经在组件层把 mbedTLS 需要的几个平台依赖拆开并预适配好了：

1. TLS 连接的网络 BIO 使用 `mbedtls_ssl_set_bio()` 绑定到 `mbedtls_net_send()` / `mbedtls_net_recv()`。
2. `mbedtls_net_send()` / `mbedtls_net_recv()` 在 ESP-IDF 的 `components/mbedtls/port/net_sockets.c` 中实现，内部调用 `read()` / `write()` / `select()` / `fcntl()` 等 POSIX/BSD socket 风格接口。
3. ESP-IDF 的 lwIP + VFS 层把 socket fd 注册成标准 fd，使 `read()`、`write()`、`close()`、`fcntl()`、`select()` 等标准名字能操作 lwIP socket。
4. mbedTLS 的内存、随机数、时间等平台依赖由 ESP-IDF 的 mbedTLS port 编译期配置或 `esp-tls` 每个连接内部初始化，不需要用户再调用一个全局 `tuya_tls_init()`。

所以，对 TLS 读写链路来说，最关键的适配点是 `mbedtls_ssl_set_bio()` 所需的 `f_send` / `f_recv` / `f_recv_timeout` 回调，以及这些回调背后的 socket/POSIX 接口。

## TuyaOpen-xiaozhi 的实现方式

TuyaOpen-xiaozhi 的 `tuya_tls_init()` 位于 `src/tuya_cloud_service/tls/tuya_tls.c`，由 `tuya_iot_init()` 调用。

它主要做三类全局初始化：

- 调用 `mbedtls_threading_set_alt()`，把 mbedTLS 锁适配到 `tal_mutex_*`。
- 调用 `mbedtls_platform_set_calloc_free()`，把 mbedTLS 内存分配适配到 `tal_calloc/tal_free` 或 PSRAM 分配器。
- 初始化全局 `mbedtls_entropy_context` 和 `mbedtls_ctr_drbg_context`，并用 `mbedtls_ctr_drbg_seed()` 建立随机数源。

TLS 连接建立时，TuyaOpen 不是让 mbedTLS 创建 socket。头文件 `tuya_tls.h` 也明确说明：mbedTLS 只用于加密会话，不用于创建会话。调用方先创建 socket，再把 `socket_fd` 传入 `tuya_tls_connect()`。

在握手阶段，TuyaOpen 调用：

```c
mbedtls_ssl_set_bio(p_ssl_ctx,
                    tls_context,
                    __tuya_tls_socket_send_cb,
                    __tuya_tls_socket_recv_cb,
                    NULL);
```

其中：

- `__tuya_tls_socket_send_cb()` 调 `tal_net_send()`。
- `__tuya_tls_socket_recv_cb()` 调 `tal_net_select()`、`tal_net_recv()`，并临时切换阻塞/非阻塞状态。

这些 `tal_net_*` 再通过 TAL/TKL 网络抽象落到平台实现。Linux/POSIX 版本在 `src/tal_network/src/tal_posix.c`，会调用 `socket()`、`connect()`、`send()`、`recv()`、`select()`、`fcntl()`、`setsockopt()`、`errno` 等接口。

握手完成后，如果用户配置了 `config.f_send` / `config.f_recv`，TuyaOpen 会再次调用 `mbedtls_ssl_set_bio()`，把 BIO 切换到用户自定义回调。

## ESP-IDF 的实现方式

ESP-IDF 的 TLS 上层入口是 `esp-tls`。`esp_tls_init()` 会分配 `esp_tls_t`，并调用 `esp_mbedtls_net_init()` 初始化 `mbedtls_net_context`。真正连接时，`esp_tls.c` 先通过标准 BSD socket 风格接口创建 TCP 连接：

- `getaddrinfo()`
- `socket()`
- `setsockopt()`
- `fcntl()`
- `connect()`
- `select()`
- `getsockopt()`

TCP 连接建立后，`esp_tls_mbedtls.c` 初始化每个连接自己的 mbedTLS 上下文：

- `mbedtls_ssl_init()`
- `mbedtls_ssl_config_init()`
- `mbedtls_entropy_init()`
- `mbedtls_ctr_drbg_init()`
- `mbedtls_ctr_drbg_seed()`
- `mbedtls_ssl_conf_rng()`
- `mbedtls_ssl_setup()`

随后绑定 BIO：

```c
mbedtls_ssl_set_bio(&tls->ssl,
                    &tls->server_fd,
                    mbedtls_net_send,
                    mbedtls_net_recv,
                    NULL);
```

这里的 `tls->server_fd.fd` 已经被设置为底层 TCP socket fd。

需要注意一个容易误判的点：ESP-IDF 的 `mbedtls/esp_config.h` 中显式 `#undef MBEDTLS_NET_C`，并没有直接使用上游 `mbedtls/library/net_sockets.c`。ESP-IDF 在 `components/mbedtls/CMakeLists.txt` 中移除了上游 `net_sockets.c`，改用 `components/mbedtls/port/net_sockets.c`。这个 port 文件即使 `MBEDTLS_NET_C` 未定义也会提供 `mbedtls_net_init/connect/bind/send/recv/recv_timeout/free` 等符号。

## ESP-IDF 的 POSIX/BSD socket 适配

ESP-IDF 的 lwIP 文档列出的 BSD socket API 包括：

- `socket()`
- `bind()`
- `accept()`
- `shutdown()`
- `getpeername()`
- `getsockopt()` / `setsockopt()`
- `close()`
- `read()` / `readv()` / `write()` / `writev()`
- `recv()` / `recvmsg()` / `recvfrom()`
- `send()` / `sendmsg()` / `sendto()`
- `select()`
- `poll()`
- `fcntl()`
- 非标准：`ioctl()`

实现上，ESP-IDF 的关键层次是：

1. lwIP socket API
   - `components/lwip/lwip/src/api/sockets.c`
   - 提供 `lwip_socket()`、`lwip_connect()`、`lwip_recv()`、`lwip_send()`、`lwip_select()`、`lwip_fcntl()` 等。

2. lwIP 配置
   - `components/lwip/port/include/lwipopts.h`
   - `LWIP_SOCKET` 为 1，启用 socket API。
   - `LWIP_COMPAT_SOCKETS` 为 0，ESP-IDF 不主要依赖 lwIP 头文件宏把 `socket` 直接替换成 `lwip_socket`。
   - `LWIP_POSIX_SOCKETS_IO_NAMES` 为 0，`read/write/close/fcntl` 通过 VFS 层映射。

3. VFS socket fd 注册
   - `components/lwip/port/esp32xx/vfs_lwip.c`
   - `esp_vfs_lwip_sockets_register()` 把 lwIP socket fd 范围注册到 VFS。
   - VFS 回调中 `.read = lwip_read`、`.write = lwip_write`、`.close = lwip_close`、`.fcntl = lwip_fcntl_r_wrapper`、`.ioctl = lwip_ioctl_r_wrapper`。
   - 启用 `CONFIG_VFS_SUPPORT_SELECT` 时，`.socket_select = lwip_select`，标准 `select()` 由 VFS 分发到 socket select。

4. lwIP FreeRTOS 系统层
   - `components/lwip/port/freertos/sys_arch.c`
   - 提供 lwIP 需要的信号量、邮箱、线程、tick、互斥保护等 OS 抽象。

因此，ESP-IDF 的“POSIX 适配”准确地说包含两部分：

- socket 层暴露 BSD/POSIX 风格 API 给上层代码和 mbedTLS port。
- VFS 层把 socket fd 纳入标准 fd 模型，让 `read/write/close/fcntl/select` 能对 socket 工作。

## mbedTLS 真正依赖的接口

如果只看 TLS client 常见路径，核心依赖如下。

### mbedTLS BIO 层

`mbedtls_ssl_set_bio()` 需要：

- `f_send(void *ctx, const unsigned char *buf, size_t len)`
- `f_recv(void *ctx, unsigned char *buf, size_t len)`
- 可选 `f_recv_timeout(void *ctx, unsigned char *buf, size_t len, uint32_t timeout)`

TuyaOpen 用 `__tuya_tls_socket_send_cb()` / `__tuya_tls_socket_recv_cb()` 适配到 `tal_net_*`。

ESP-IDF 用 `mbedtls_net_send()` / `mbedtls_net_recv()` 适配到 socket fd。

### ESP-IDF mbedTLS net_sockets port 依赖

`components/mbedtls/port/net_sockets.c` 主要依赖：

- DNS/建连：`getaddrinfo()`、`freeaddrinfo()`、`socket()`、`connect()`、`bind()`、`listen()`、`accept()`、`getsockname()`、`recvfrom()`
- socket 选项：`setsockopt()`、`getsockopt()`
- 读写：`read()`、`write()`
- 关闭：`shutdown()`、`close()`
- 阻塞控制：`fcntl()`、`O_NONBLOCK`
- 等待/超时：`select()`、`fd_set`、`struct timeval`
- 错误语义：`errno`、`EAGAIN`、`EWOULDBLOCK`、`EINTR`、`EPIPE`、`ECONNRESET`

如果平台不能提供这些接口，就要像 TuyaOpen 一样绕开 `mbedtls_net_*`，自己实现 `f_send/f_recv` 回调。

## 为什么 ESP-IDF 不需要 tuya_tls_init

原因分层看更清楚：

1. 网络 BIO 不需要应用注册
   - ESP-IDF 在 `esp_tls_mbedtls.c` 内部固定绑定 `mbedtls_net_send/recv`。
   - `mbedtls_net_send/recv` 又由 ESP-IDF 的 mbedTLS port 提供。
   - 这些函数背后的 POSIX/BSD socket API 已经由 lwIP + VFS 支撑。

2. 随机数不需要全局 `tuya_tls_init`
   - ESP-IDF 在每个 `esp_tls_t` 连接中初始化 entropy/CTR_DRBG。
   - 硬件随机源通过 `MBEDTLS_ENTROPY_HARDWARE_ALT` 接到 `esp_fill_random()`。

3. 内存分配不需要运行时注册
   - ESP-IDF 在 `mbedtls/esp_config.h` 中定义 `MBEDTLS_PLATFORM_STD_CALLOC` 和 `MBEDTLS_PLATFORM_STD_FREE` 到 `esp_mbedtls_mem_calloc/free`。
   - 因此不需要应用调用 `mbedtls_platform_set_calloc_free()`。

4. 线程/锁由组件配置和 port 处理
   - ESP-IDF mbedTLS port、ROM 适配和 FreeRTOS/lwIP 系统层提供所需同步原语。
   - TuyaOpen 因为要跨多平台运行，选择在 SDK 初始化时用 `mbedtls_threading_set_alt()` 注册 TAL mutex。

## 迁移判断

如果把 TuyaOpen-xiaozhi 移植到 ESP-IDF，有两种路线：

1. 保留 TuyaOpen 的 `tuya_tls.c`
   - 需要确保 `tal_net_*` 在 ESP-IDF 平台下正确映射到 ESP-IDF socket API。
   - 重点适配 `tal_net_send`、`tal_net_recv`、`tal_net_select`、`tal_net_set_block/get_nonblock`、`tal_net_set_timeout`、`tal_net_get_errno`。
   - 这种方式改动小，但会保留 Tuya TLS/TAL 网络抽象。

2. 改用 ESP-IDF `esp-tls`
   - 可以去掉 TuyaOpen 自己的 mbedTLS 初始化和 BIO 回调。
   - 需要把 Tuya TLS 配置、PSK/证书、读写接口映射到 `esp_tls_cfg_t` 和 `esp_tls_conn_*`。
   - 这种方式更贴近 ESP-IDF，但对 Tuya 云 SDK 的接口侵入更大。

若目标是解释“ESP-IDF 为什么不需要 `tuya_tls_init()`”，最准确的答案是：ESP-IDF 已经在组件内部完成了 mbedTLS 平台 port，其中网络路径依赖 lwIP/VFS 提供的 POSIX/BSD socket 语义；而 TuyaOpen 为了跨平台，必须在 SDK 层显式注册 mutex、内存、随机数以及 TLS BIO 网络回调。
