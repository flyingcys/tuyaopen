# ESP-IDF mbedTLS 硬件加速适配分析

## 结论

ESP-IDF 的 mbedTLS 硬件加速不是在 TLS 层单独做一套特殊接口，而是在 `components/mbedtls` 组件里通过三层机制接入：

1. Kconfig 决定是否启用某类硬件加速，例如 `CONFIG_MBEDTLS_HARDWARE_AES`、`CONFIG_MBEDTLS_HARDWARE_SHA`、`CONFIG_MBEDTLS_HARDWARE_MPI`。
2. `mbedtls/esp_config.h` 根据这些配置打开 mbedTLS 的 `*_ALT` 宏，让 mbedTLS 编译时使用 ESP-IDF 提供的替代实现。
3. `components/mbedtls/CMakeLists.txt` 根据 Kconfig 和 SoC capability 宏，把对应 port 源文件编入 `mbedcrypto`，再由这些 port 文件调用 HAL/LL、ROM、efuse、DS/ECDSA 外设或 DMA/GDMA。

因此，TLS 代码本身一般不需要知道 AES/SHA/RSA/ECC 是否走硬件。只要 TLS 握手、证书验证、密钥交换、记录层加解密调用的是 mbedTLS 标准 API，最终就会自动落到硬件加速实现或软件回退实现。

## 入口配置

主要配置位于 `esp-idf/components/mbedtls/Kconfig`。

常见硬件加速配置如下：

- `CONFIG_MBEDTLS_HARDWARE_AES`
  - 默认启用，依赖 `SOC_AES_SUPPORTED`。
  - 启用 AES 加解密硬件加速。

- `CONFIG_MBEDTLS_HARDWARE_GCM`
  - 依赖 `SOC_AES_SUPPORT_GCM && MBEDTLS_HARDWARE_AES`。
  - 启用部分 AES-GCM 硬件加速。代码注释说明 GHASH 仍由软件计算。

- `CONFIG_MBEDTLS_HARDWARE_MPI`
  - 默认启用，依赖 `SOC_MPI_SUPPORTED`。
  - 加速大数乘法、模乘、模幂，主要服务 RSA。

- `CONFIG_MBEDTLS_LARGE_KEY_SOFTWARE_MPI`
  - 当硬件最大 RSA 位宽有限时，允许超过 `SOC_RSA_MAX_BIT_LEN` 的 key 回退软件。

- `CONFIG_MBEDTLS_HARDWARE_SHA`
  - 默认启用，依赖 `SOC_SHA_SUPPORTED`。
  - 加速 SHA1、SHA256、以及 SoC 支持时的 SHA384/SHA512。

- `CONFIG_MBEDTLS_HARDWARE_ECC`
  - 默认启用，依赖 `SOC_ECC_SUPPORTED`。
  - 加速 ECC 点乘和公钥点校验，主要支持 SECP192R1、SECP256R1，部分 SoC 支持 P384。

- `CONFIG_MBEDTLS_ECC_OTHER_CURVES_SOFT_FALLBACK`
  - 硬件不支持的曲线回退软件。

- `CONFIG_MBEDTLS_HARDWARE_ECDSA_SIGN`
  - 默认关闭，依赖 `SOC_ECDSA_SUPPORTED`。
  - 使用片上 ECDSA 外设签名，私钥必须烧录到 efuse key block，并设置正确 key purpose。

- `CONFIG_MBEDTLS_HARDWARE_ECDSA_VERIFY`
  - 默认启用，依赖 `SOC_ECDSA_SUPPORTED`。
  - 使用片上 ECDSA 外设验签。

- `CONFIG_MBEDTLS_ROM_MD5`
  - 默认启用。
  - 使用 ROM 中的 MD5 实现。

- `CONFIG_MBEDTLS_USE_CRYPTO_ROM_IMPL`
  - 使用 ROM 中的部分 mbedTLS crypto 实现来节省 flash。
  - 会选择启用 SHA512、AES、CCM、CMAC、ROM MD5、硬件 SHA、ECP restartable、threading 等配置。

这些配置还会受到 SoC capability 限制，例如：

- `SOC_AES_SUPPORTED`
- `SOC_AES_SUPPORT_DMA`
- `SOC_AES_GDMA`
- `SOC_AES_SUPPORT_GCM`
- `SOC_SHA_SUPPORTED`
- `SOC_SHA_SUPPORT_DMA`
- `SOC_SHA_SUPPORT_PARALLEL_ENG`
- `SOC_SHA_GDMA`
- `SOC_MPI_SUPPORTED`
- `SOC_RSA_MAX_BIT_LEN`
- `SOC_ECC_SUPPORTED`
- `SOC_ECDSA_SUPPORTED`
- `SOC_DIG_SIGN_SUPPORTED`
- `SOC_HMAC_SUPPORTED`

## mbedTLS ALT 宏接管

`esp-idf/components/mbedtls/port/include/mbedtls/esp_config.h` 是 ESP-IDF 的 mbedTLS 配置总入口。

它根据 Kconfig 打开如下替代实现宏：

- AES
  - `MBEDTLS_AES_ALT`
  - `MBEDTLS_GCM_ALT`
  - `MBEDTLS_GCM_NON_AES_CIPHER_SOFT_FALLBACK`

- SHA
  - `MBEDTLS_SHA1_ALT`
  - `MBEDTLS_SHA256_ALT`
  - `MBEDTLS_SHA512_ALT`，仅在 `SOC_SHA_SUPPORT_SHA512` 时启用。

- MD5
  - `MBEDTLS_MD5_ALT`

- MPI/bignum
  - `MBEDTLS_MPI_EXP_MOD_ALT`
  - `MBEDTLS_MPI_EXP_MOD_ALT_FALLBACK`
  - `MBEDTLS_MPI_MUL_MPI_ALT`

- ECC/ECP
  - `MBEDTLS_ECP_MUL_ALT`
  - `MBEDTLS_ECP_MUL_ALT_SOFT_FALLBACK`
  - `MBEDTLS_ECP_VERIFY_ALT`
  - `MBEDTLS_ECP_VERIFY_ALT_SOFT_FALLBACK`

- 外部 ECDSA 芯片 ATECC608A
  - `MBEDTLS_ECDSA_SIGN_ALT`
  - `MBEDTLS_ECDSA_VERIFY_ALT`

- 熵源
  - `MBEDTLS_ENTROPY_HARDWARE_ALT`

这些宏的意义是：mbedTLS 上层仍调用标准函数名，但链接/编译进来的实现已经换成 ESP-IDF port。比如 TLS 记录层调用 `mbedtls_aes_crypt_cbc()`，实际可进入 `esp_aes_crypt_cbc()`；证书验证调用 SHA 或 ECDSA，实际可进入 ESP 的 SHA/ECDSA port。

## CMake 选源机制

`esp-idf/components/mbedtls/CMakeLists.txt` 负责把具体后端文件加入 `mbedcrypto`。

它先根据 SoC 能力选择外设类型：

- SHA
  - `CONFIG_SOC_SHA_SUPPORT_PARALLEL_ENG` 为真时，使用 `port/sha/parallel_engine`。
  - 否则使用 `port/sha/core`。

- AES
  - `CONFIG_SOC_AES_SUPPORT_DMA` 为真时，使用 `port/aes/dma`。
  - 否则使用 `port/aes/block`。

然后按配置追加源文件：

- AES 基础硬件实现
  - `port/aes/esp_aes_common.c`
  - `port/aes/esp_aes_xts.c`
  - `port/aes/block/esp_aes.c` 或 `port/aes/dma/esp_aes.c`

- AES DMA
  - `port/aes/dma/esp_aes_dma_core.c`
  - `port/aes/dma/esp_aes_crypto_dma_impl.c` 或 `port/aes/dma/esp_aes_gdma_impl.c`

- AES-GCM
  - `port/aes/esp_aes_gcm.c`

- SHA
  - `port/sha/esp_sha.c`
  - `port/sha/core/sha.c` 或 `port/sha/parallel_engine/sha.c`
  - `port/sha/core/esp_sha1.c`、`esp_sha256.c`、`esp_sha512.c`
  - 或 `port/sha/parallel_engine/esp_sha1.c`、`esp_sha256.c`、`esp_sha512.c`

- SHA DMA/GDMA
  - `port/sha/core/esp_sha_crypto_dma_impl.c`
  - `port/sha/core/esp_sha_gdma_impl.c`

- 共享 GDMA
  - `port/crypto_shared_gdma/esp_crypto_shared_gdma.c`

- MPI/RSA 大数
  - `port/bignum/esp_bignum.c`
  - `port/bignum/bignum_alt.c`

- ECC
  - `port/ecc/esp_ecc.c`
  - `port/ecc/ecc_alt.c`

- ECDSA
  - `port/ecdsa/ecdsa_alt.c`
  - 并通过链接器 `--wrap` 包装 `mbedtls_ecdsa_sign`、`mbedtls_ecdsa_verify`、`mbedtls_ecdsa_write_signature`、`mbedtls_ecdsa_read_signature` 等函数。

- DS 数字签名外设
  - `port/esp_ds/esp_rsa_sign_alt.c`
  - `port/esp_ds/esp_rsa_dec_alt.c`
  - `port/esp_ds/esp_ds_common.c`

- 硬件随机源
  - `port/esp_hardware.c`

- ROM crypto
  - `port/mbedtls_rom/mbedtls_rom_osi.c`

## AES 适配

AES 的 mbedTLS 接管点是 `MBEDTLS_AES_ALT`。ESP-IDF 的 AES context 和函数由 `port/include/aes_alt.h`、`port/include/aes/esp_aes.h` 和 `port/aes/*` 提供。

### block 后端

当 SoC 没有 AES DMA 支持时，CMake 使用：

- `port/aes/block/esp_aes.c`

典型流程：

1. `esp_aes_acquire_hardware()` 获取 AES spinlock，打开 AES bus clock，reset 寄存器。
2. `aes_hal_setkey()` 写入 key 和加/解密方向。
3. `aes_hal_transform_block()` 执行 16 字节 block 运算。
4. `esp_aes_release_hardware()` 关闭 clock 并释放锁。

代码中还有故障注入检查：如果输出和输入完全一致，会清零输出并 `abort()`，防止硬件操作被跳过。

### DMA 后端

当 `SOC_AES_SUPPORT_DMA` 为真时，CMake 使用：

- `port/aes/dma/esp_aes.c`
- `port/aes/dma/esp_aes_dma_core.c`
- `port/aes/dma/esp_aes_crypto_dma_impl.c` 或 `port/aes/dma/esp_aes_gdma_impl.c`

典型流程：

1. `esp_aes_acquire_hardware()` 获取 AES/SHA 共享锁或 crypto DMA 锁，打开 AES clock。
2. `aes_hal_setkey()` 写 key。
3. `aes_hal_mode_init()` 选择 ECB/CBC/CTR/OFB/CFB/GCM 等模式。
4. `esp_aes_process_dma()` 或 `esp_aes_process_dma_gcm()` 构造 DMA descriptor 并启动 DMA。
5. 根据配置可使用中断等待，或轮询 DMA 完成。
6. 释放硬件。

若使用 GDMA，AES 和 SHA 共享 `esp_crypto_shared_gdma.c`。该文件会在每次启动前把 TX channel 重新连接到 AES 或 SHA 触发源，因为同一个 TX channel 在两个外设之间复用。

## AES-GCM 适配

AES-GCM 的接管点是 `MBEDTLS_GCM_ALT`，实现位于：

- `port/aes/esp_aes_gcm.c`

其特点：

- AES-CTR 加/解密部分可以走 AES 硬件或 DMA。
- GHASH 仍由软件计算。
- 当 `CONFIG_MBEDTLS_GCM_SUPPORT_NON_AES_CIPHER` 启用时，非 AES cipher 的 GCM 会回退到软件实现。
- `esp_aes_gcm_crypt_and_tag()`、`esp_aes_gcm_auth_decrypt()` 等函数内部会判断是否使用软件 context 或硬件路径。

所以 `CONFIG_MBEDTLS_HARDWARE_GCM` 是“部分硬件加速”，不是完整 GCM 全部硬件化。

## SHA 适配

SHA 的接管点是：

- `MBEDTLS_SHA1_ALT`
- `MBEDTLS_SHA256_ALT`
- `MBEDTLS_SHA512_ALT`

实现分两类。

### core 后端

路径：

- `port/sha/core/sha.c`
- `port/sha/core/esp_sha1.c`
- `port/sha/core/esp_sha256.c`
- `port/sha/core/esp_sha512.c`

`sha.c` 提供硬件操作基础函数：

- `esp_sha_acquire_hardware()`
- `esp_sha_release_hardware()`
- `esp_sha_set_mode()`
- `esp_sha_block()`
- `esp_sha_read_digest_state()`
- `esp_sha_write_digest_state()`

如果 SoC 支持 SHA DMA，则输入长度超过阈值时使用 DMA。阈值在 `port/sha/core/include/esp_sha_internal.h`：

- ESP32P4：512 字节
- ESP32S3：256 字节
- 其他：128 字节

小数据走 block mode，大数据走 DMA mode，避免小数据场景被 DMA descriptor 开销拖慢。

### parallel_engine 后端

路径：

- `port/sha/parallel_engine/sha.c`
- `port/sha/parallel_engine/esp_sha1.c`
- `port/sha/parallel_engine/esp_sha256.c`
- `port/sha/parallel_engine/esp_sha512.c`

该后端维护多个 SHA engine 的占用状态，并使用 semaphore 控制并发：

- SHA1 engine
- SHA2_256 engine
- SHA2_384/SHA2_512 engine

如果某个 SHA context 无法拿到硬件 engine，会自动转入软件计算模式。这就是 Kconfig 中提到的“多个 SHA digest 同时计算时，部分会自动软件回退”。

## MPI/RSA 大数适配

MPI 的接管点是：

- `MBEDTLS_MPI_EXP_MOD_ALT`
- `MBEDTLS_MPI_EXP_MOD_ALT_FALLBACK`
- `MBEDTLS_MPI_MUL_MPI_ALT`

实现位于：

- `port/bignum/esp_bignum.c`
- `port/bignum/bignum_alt.c`

这些文件调用 MPI/RSA 硬件 HAL：

- `mpi_hal_set_mode()`
- `mpi_hal_write_to_mem_block()`
- `mpi_hal_start_op()`
- `mpi_hal_wait_op_complete()`
- `mpi_hal_read_result_hw_op()`

主要加速：

- `mbedtls_mpi_mul_mpi()`
- `mbedtls_mpi_exp_mod()`
- ESP-IDF 扩展的 `esp_mpi_mul_mpi_mod()`

`CONFIG_MBEDTLS_LARGE_KEY_SOFTWARE_MPI` 启用时，`MBEDTLS_MPI_EXP_MOD_ALT_FALLBACK` 会优先尝试硬件，超过硬件能力范围时回退 `mbedtls_mpi_exp_mod_soft()`。这对 RSA key 长度很关键，因为硬件最大位宽由 `SOC_RSA_MAX_BIT_LEN` 决定，例如部分 SoC 是 3072 bit。

如果启用 `CONFIG_MBEDTLS_MPI_USE_INTERRUPT`，长时间 MPI 模幂运算可使用中断等待完成，并配合 PM lock 防止 CPU 频率变化或 light sleep 影响操作。

## ECC/ECP 适配

ECC 的接管点是：

- `MBEDTLS_ECP_MUL_ALT`
- `MBEDTLS_ECP_MUL_ALT_SOFT_FALLBACK`
- `MBEDTLS_ECP_VERIFY_ALT`
- `MBEDTLS_ECP_VERIFY_ALT_SOFT_FALLBACK`

实现位于：

- `port/ecc/ecc_alt.c`
- `port/ecc/esp_ecc.c`

`ecc_alt.c` 替换 mbedTLS 的 ECP 点乘和公钥点校验入口：

- `ecp_mul_restartable_internal()`
- `mbedtls_ecp_check_pubkey()`

硬件调用链：

```text
mbedtls ECP API
  -> ecc_alt.c
  -> esp_ecc_point_multiply() / esp_ecc_point_verify()
  -> ecc_hal_write_mul_param() / ecc_hal_write_verify_param()
  -> ecc_hal_start_calc()
  -> ecc_hal_read_*_result()
```

支持曲线主要是：

- `MBEDTLS_ECP_DP_SECP192R1`
- `MBEDTLS_ECP_DP_SECP256R1`
- 部分 SoC 支持 `MBEDTLS_ECP_DP_SECP384R1`

启用 `CONFIG_MBEDTLS_ECC_OTHER_CURVES_SOFT_FALLBACK` 时，硬件不支持的曲线会回退软件。否则会返回 `MBEDTLS_ERR_ECP_BAD_INPUT_DATA`。

## ECDSA 适配

ECDSA 比普通 ECC 更特殊。ESP-IDF 没有仅依赖 `MBEDTLS_ECDSA_SIGN_ALT` 接管片上 ECDSA 外设，而是在 CMake 中对 mbedTLS ECDSA 函数做链接器包装：

- `-Wl,--wrap=mbedtls_ecdsa_sign`
- `-Wl,--wrap=mbedtls_ecdsa_sign_restartable`
- `-Wl,--wrap=mbedtls_ecdsa_write_signature`
- `-Wl,--wrap=mbedtls_ecdsa_write_signature_restartable`
- `-Wl,--wrap=mbedtls_ecdsa_verify`
- `-Wl,--wrap=mbedtls_ecdsa_verify_restartable`
- `-Wl,--wrap=mbedtls_ecdsa_read_signature`
- `-Wl,--wrap=mbedtls_ecdsa_read_signature_restartable`

实现位于：

- `port/ecdsa/ecdsa_alt.c`

### ECDSA 签名

`CONFIG_MBEDTLS_HARDWARE_ECDSA_SIGN` 默认关闭。启用后，私钥不能是普通内存 key，而是必须放在 efuse key block 或 key manager 中。

ESP-IDF 用一个特殊的 `mbedtls_mpi` 标记私钥来源：

- `d.s = ECDSA_KEY_MAGIC`
- `d.n = efuse block id`
- `d.p = NULL`

这样 `__wrap_mbedtls_ecdsa_sign()` 能识别“这是硬件 ECDSA key”，并调用 `esp_ecdsa_sign()`。

签名前会校验 efuse key purpose 是否为对应 ECDSA key purpose。然后调用：

```text
esp_ecdsa_sign()
  -> esp_ecdsa_acquire_hardware()
  -> ecdsa_hal_gen_signature()
  -> ecdsa_hal_get_operation_result()
  -> mbedtls_mpi_read_binary_le(r/s)
  -> esp_ecdsa_release_hardware()
```

ESP32-H2 等目标上还可启用签名侧信道 countermeasure，例如 dummy sign masking 和固定时间延迟。

### ECDSA 验签

`CONFIG_MBEDTLS_HARDWARE_ECDSA_VERIFY` 默认启用。验签包装函数会在曲线和 hash 长度满足硬件要求时调用 `esp_ecdsa_verify()`，否则回到 mbedTLS 原始软件实现。

典型支持：

- SECP192R1 + 32 字节 hash
- SECP256R1 + 32 字节 hash
- 部分 SoC 支持 P384 + 48 字节 hash

### TEE secure storage ECDSA

`CONFIG_MBEDTLS_TEE_SEC_STG_ECDSA_SIGN` 启用时，`ecdsa_alt.c` 还支持从 TEE secure storage 中加载 ECDSA key。它使用另一种 magic 标记：

- `ECDSA_KEY_MAGIC_TEE`

包装函数识别后会走 TEE secure storage 的签名路径。

## DS/RSA 私钥保护适配

DS 即 Digital Signature 外设，路径：

- `port/esp_ds/esp_ds_common.c`
- `port/esp_ds/esp_rsa_sign_alt.c`
- `port/esp_ds/esp_rsa_dec_alt.c`

它不是普通的 MPI 加速。MPI 加速处理一般 RSA 大数运算；DS 用于“私钥不出硬件/efuse 保护域”的 RSA 签名或解密。

典型流程：

1. 上层通过 `esp_ds_init_data_ctx()` 设置 DS 参数和 HMAC key id。
2. `esp_ds_common.c` 保存全局 DS data context，并用 mutex 保证同一时间只有一个 DS 会话。
3. `esp_ds_rsa_sign()` 先按 RSA padding 规则构造待签名块。
4. 调用 `esp_ds_start_sign()` 和 `esp_ds_finish_sign()` 让 DS 外设完成私钥运算。
5. 输出转换回 mbedTLS 期望的字节序。

这类适配常用于 TLS 客户端证书私钥存放在 efuse/DS 参数中的场景。

## 硬件随机数与熵源

熵源接管点是：

- `MBEDTLS_ENTROPY_HARDWARE_ALT`

实现位于：

- `port/esp_hardware.c`

核心函数：

```c
int mbedtls_hardware_poll(void *data,
                          unsigned char *output,
                          size_t len,
                          size_t *olen)
{
    esp_fill_random(output, len);
    *olen = len;
    return 0;
}
```

因此 `mbedtls_entropy_init()` 注册默认 entropy source 后，`mbedtls_ctr_drbg_seed()` 会通过 ESP-IDF 的硬件随机源拿熵。`esp-tls` 每个连接中初始化 `mbedtls_entropy_context` 和 `mbedtls_ctr_drbg_context` 时，就会自动走这条路径。

## 锁、时钟与 DMA 资源管理

ESP-IDF 的硬件加速 port 都会显式管理外设互斥和时钟：

- AES block 后端使用 spinlock，操作短，避免重锁开销。
- AES DMA 后端使用 `esp_crypto_sha_aes_lock_acquire()` 或 `esp_crypto_dma_lock_acquire()`。
- SHA core 后端使用 `esp_crypto_sha_aes_lock_acquire()`。
- SHA parallel engine 后端按 engine 类型维护 semaphore，同时用 spinlock 保护共享内存块。
- MPI 使用 `esp_crypto_mpi_lock_acquire()`，并可配合中断和 PM lock。
- ECC/ECDSA 使用各自的 crypto lock，并打开/关闭 ECC、ECDSA、MPI 等相关外设 clock。
- GDMA 后端通过 `esp_crypto_shared_gdma_start*()` 在 AES/SHA 间复用 DMA channel。

这解释了为什么应用层不需要手动初始化 AES/SHA/RSA 硬件：锁和 clock 生命周期在每次 mbedTLS 算法调用内部完成。

## TLS 中哪些地方会受益

在 TLS 通信中，硬件加速主要影响这些阶段：

- 握手随机数
  - `mbedtls_ctr_drbg_seed()` 使用 `esp_fill_random()` 熵源。

- 证书链验证
  - SHA256/SHA384 digest 可能走 SHA 硬件。
  - RSA 签名验证中的 MPI 模幂可能走 MPI/RSA 硬件。
  - ECDSA 验签可能走 ECDSA 或 ECC 硬件。

- ECDHE 密钥交换
  - ECP 点乘可能走 ECC 硬件。

- 客户端证书签名
  - RSA 私钥如果走 DS，会使用 DS 外设。
  - ECDSA 私钥如果在 efuse/key manager/TEE secure storage，可走片上 ECDSA 或 TEE。

- 应用数据加解密
  - AES-CBC、AES-CTR、AES-GCM 等可能走 AES 硬件/DMA。
  - AES-GCM 中 AES 部分硬件加速，GHASH 软件计算。

## 与 TuyaOpen 移植的关系

如果 TuyaOpen-xiaozhi 继续使用自己的 `tuya_tls.c`，只要链接的是 ESP-IDF 的 mbedTLS 组件，并且 mbedTLS config 使用 `mbedtls/esp_config.h`，算法层硬件加速仍会生效。原因是 Tuya TLS 调用的仍是标准 mbedTLS API：

- `mbedtls_ssl_handshake()`
- `mbedtls_ssl_read()`
- `mbedtls_ssl_write()`
- `mbedtls_ssl_conf_rng()`
- `mbedtls_x509_crt_parse()`
- `mbedtls_ssl_conf_psk()`

这些 API 内部再调用 AES/SHA/MPI/ECC/ECDSA 等模块时，会自动进入 ESP-IDF 的 `ALT` 实现。

需要注意：

1. 不要把 TuyaOpen 自带的 `src/libtls/mbedtls-3.1.0` 和 ESP-IDF 的 mbedTLS 混用到同一个最终链接里。
2. 如果继续使用 TuyaOpen 自带 mbedTLS，就不会自动获得 ESP-IDF 的硬件加速 port，除非把 ESP-IDF 的 `esp_config.h`、port 源文件、HAL/LL/锁/DMA 依赖一起移植过去，工作量很大。
3. 如果目标平台是 ESP-IDF，推荐使用 ESP-IDF 的 mbedTLS 组件作为唯一 mbedTLS 实现，让 Tuya TLS 只保留上层 BIO/证书/PSK 封装。
4. 对 ECDSA hardware sign 和 DS 这类“私钥在硬件保护域”的功能，单纯打开 mbedTLS 加速还不够，上层必须按 ESP-IDF 的 API 准备 key context 或 DS data context。

## 关键路径索引

- 配置入口：`esp-idf/components/mbedtls/Kconfig`
- mbedTLS 配置头：`esp-idf/components/mbedtls/port/include/mbedtls/esp_config.h`
- CMake 选源：`esp-idf/components/mbedtls/CMakeLists.txt`
- AES：`esp-idf/components/mbedtls/port/aes/`
- SHA：`esp-idf/components/mbedtls/port/sha/`
- MPI/RSA：`esp-idf/components/mbedtls/port/bignum/`
- ECC：`esp-idf/components/mbedtls/port/ecc/`
- ECDSA：`esp-idf/components/mbedtls/port/ecdsa/`
- DS/RSA 私钥保护：`esp-idf/components/mbedtls/port/esp_ds/`
- 共享 GDMA：`esp-idf/components/mbedtls/port/crypto_shared_gdma/`
- 硬件熵源：`esp-idf/components/mbedtls/port/esp_hardware.c`

