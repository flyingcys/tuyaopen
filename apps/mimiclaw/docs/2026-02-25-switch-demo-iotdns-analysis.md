# switch_demo 中 iotdns 使用路径分析

## 1. 结论先行

- `switch_demo` 本身没有直接调用 `tuya_iotdns_*`，而是通过 `tuya_iot` 状态机和 `tuya_cloud_service` 底层间接使用。
- 该方案的核心是：
  1. 先用 iotdns 拉取 Tuya 云端点（ATOP/MQTT 域名 + 证书），再启动 MQTT/ATOP。
  2. 对“任意 HTTPS 目标”再按需通过 iotdns 查询该目标证书（如 `tuya_http`、OTA）。
- 它不依赖 esp-idf 的全局证书包模式（`crt_bundle_attach`），而是依赖 iotdns 动态下发证书。

## 2. switch_demo 入口与触发点

### 2.1 入口

- `switch_demo` 入口在 `user_main()`：`tuyaopen/apps/tuya_cloud/switch_demo/src/tuya_main.c:214`
- 在入口中执行：
  - `tuya_iot_init(...)`：`tuya_main.c:255`
  - `tuya_iot_start(...)`：`tuya_main.c:288`
  - 主循环 `tuya_iot_yield(...)`：`tuya_main.c:294`

### 2.2 tuya_iot 初始化期

- `tuya_iot_init` 中会初始化：
  - `tuya_register_center_init()`：`tuyaopen/src/tuya_cloud_service/cloud/tuya_iot.c:596`
  - `tuya_endpoint_init()`：`tuya_iot.c:598`
- `tuya_register_center_init` 会优先读 KV（`rcs.active`/`rcs.mf`），失败则回落到默认 RCS（iotdns 域名+CA）：`tuyaopen/src/tuya_cloud_service/cloud/tuya_register_center.c:333`

## 3. iotdns 的两条实际使用链

### 3.1 链路A：云端点发现（核心启动链）

### 状态机路径

- `tuya_iot_yield` 状态机：`tuya_iot.c:854`
- 关键状态：
  - `STATE_NETWORK_CHECK`：网络可用后进入 endpoint 流程：`tuya_iot.c:935`
  - `STATE_ENDPOINT_GET`：先尝试从 KV 读取 `endpoint.cert/domain`：`tuya_iot.c:944`
  - `STATE_ENDPOINT_UPDATE`：读取失败或无缓存时，调用 `tuya_endpoint_update()`：`tuya_iot.c:955`

### endpoint 更新如何触发 iotdns

- `tuya_endpoint_update()`：`tuyaopen/src/tuya_cloud_service/cloud/tuya_endpoint.c:295`
- 内部调用 `iotdns_cloud_endpoint_get(...)` 获取：
  - `httpsSelfUrl`
  - `mqttsSelfUrl`
  - `caArr`（证书）

对应实现：
- `iotdns_cloud_endpoint_get`：`tuyaopen/src/tuya_cloud_service/cloud/iotdns.c:157`
- 请求路径：`/v2/url_config`：`iotdns.c:209`
- 请求体格式：`IOTDNS_REQUEST_FMT`/`IOTDNS_REQUEST_FMT_NOREGION`：`iotdns.c:33`、`iotdns.c:37`

### endpoint 证书如何被消费

- MQTT 初始化直接使用 `endpoint->cert`：
  - `run_state_startup_update` -> `tuya_mqtt_init(... .cacert = endpoint->cert ...)`
  - 位置：`tuyaopen/src/tuya_cloud_service/cloud/tuya_iot.c:475`、`tuya_iot.c:483`
- ATOP 请求也直接使用 `endpoint->cert`：
  - `tuyaopen/src/tuya_cloud_service/cloud/atop_base.c:471`
- 绑定态（直连 MQTT 拿 token）也会走 endpoint 自动更新：
  - `tuya_endpoint_update_auto_region()`：`tuyaopen/src/tuya_cloud_service/cloud/mqtt_bind.c:164`
  - 随后 `tuya_mqtt_init(... .cacert = endpoint->cert ...)`：`mqtt_bind.c:170-171`

### 启动阶段失败行为

- `STATE_ENDPOINT_UPDATE` 失败时 sleep 1s 后继续停留该状态重试：`tuya_iot.c:955-960`
- 即：在 switch_demo 里，endpoint 拉取失败会阻塞在启动阶段（不会进入 MQTT 正常业务态）。

### 3.2 链路B：按需查询任意主机证书（非核心启动，但常见）

### API

- 主机维度证书查询：
  - `tuya_iotdns_query_host_certs(...)`：`tuyaopen/src/tuya_cloud_service/cloud/iotdns.c:290`
  - 请求路径：`/device/dns_query`：`iotdns.c:312`
- URL 维度封装：
  - `tuya_iotdns_query_domain_certs(...)`：`iotdns.c:336`

### 使用方

- `tuya_http` 的 HTTPS 请求在发送前会调用 `tuya_http_cert_load(...)`：
  - `tuyaopen/src/tuya_cloud_service/cloud/tuya_http.c:147`
  - 内部调用 `tuya_iotdns_query_host_certs(...)`：`tuya_http.c:165`
  - 对外入口 `tuya_http_client_post_simple(...)`：`tuya_http.c:204`、`tuya_http.c:243`
- OTA 下载线程会直接按固件 URL 查询证书：
  - `tuyaopen/src/tuya_cloud_service/cloud/tuya_ota.c:170`

## 4. iotdns 请求到底信任谁（RCS链）

### 4.1 默认 RCS 与安全等级

- 默认 iotdns 域名与 CA 在 `tuya_register_center.c` 中硬编码：
  - 默认域名 `h3.iot-dns.com`：`tuya_register_center.c:56`
  - 安全等级1默认 `h6.iot-dns.com`：`tuya_register_center.c:64`
  - 安全等级2/3默认 `h4.iot-dns.com`：`tuya_register_center.c:68`
  - 默认 iotdns CA：`default_iotdns_cacert`：`tuya_register_center.c:24`
- `TUYA_SECURITY_LEVEL` 默认值为 1：
  - `tuyaopen/src/tuya_cloud_service/Kconfig:4-7`

### 4.2 RCS 动态更新来源

- BLE/AP 配网 payload 里如果带 `reg`，会写入 `RCS_APP`：
  - BLE：`tuyaopen/src/tuya_cloud_service/ble/ble_netcfg.c:107`
  - AP：`tuyaopen/src/tuya_cloud_service/netcfg/ap_netcfg.c:286`
- 保存接口：`tuya_register_center_save(...)`：`tuya_register_center.c:371`
- 读取接口：`tuya_register_center_get(...)`：`tuya_register_center.c:426`

## 5. 对你当前场景的直接启示（不引入内置业务证书前提）

- switch_demo 设计本质是“iotdns 动态证书分发”模型：
  - endpoint 证书和域名来自 iotdns；
  - 业务域名证书也可来自 iotdns（`/device/dns_query`）。
- 如果你不想维护内置业务证书（如 Telegram/Discord），那就要保证：
  - RCS 可用（`h6/h4/h3.iot-dns.com` 可达且 TLS 正常）；
  - iotdns 的 `/device/dns_query` 对你的目标域名能稳定返回 `ca`。
- 也就是说，和 switch_demo 一样，关键不在“业务证书内置”，而在“iotdns 链路可靠性”。

## 6. 建议的排查顺序（按 switch_demo 经验）

1. 先确认 RCS 连接是否成功（是否能向 `h6/h4.iot-dns.com` 发起 HTTPS）。
2. 再确认 `/v2/url_config` 是否返回有效 `httpsSelfUrl/mqttsSelfUrl/caArr`。
3. 最后确认 `/device/dns_query` 对目标业务域名是否返回有效 `ca`。
4. 如果 1-3 任一失败，优先修复 iotdns 链路，而不是把问题转移到业务侧硬编码证书。

## 7. 补充：mimiclaw 中 Feishu 成功、Telegram/Discord 失败的根因

### 7.1 现象与错误码含义

- 运行日志里常见 `rt=-9`。
- `-9` 在错误码表中是 `OPRT_CJSON_GET_ERR`（JSON 取字段失败）：`tuyaopen/src/common/include/tuya_error_code.h:50`。
- iotdns 解析逻辑要求 `ca` 字段必须是非空字符串，否则返回 `OPRT_CJSON_GET_ERR`：`tuyaopen/src/tuya_cloud_service/cloud/iotdns.c:259-261`。

### 7.2 实测 iotdns 响应差异（同一接口）

使用 `https://h6.iot-dns.com/device/dns_query` 对不同 host 查询（`need_ca=true`）：

- `open.feishu.cn`：返回非空 `ca`（长度约 1544）
- `api.telegram.org`：返回 `ca:""`（空）
- `gateway.discord.gg`：返回 `ca:""`（空）
- `discord.com`：返回 `ca:""`（空）

这与当前运行现象完全一致：Feishu 可拿到证书，Telegram/Discord 拿不到。

### 7.3 这不是 Feishu 内置业务证书导致

- `mimiclaw` 的 Feishu 路径也是通过 `mimi_tls_query_domain_certs()` 调 iotdns 获取证书：`tuyaopen/apps/mimiclaw/channels/feishu_bot.c:249`、`feishu_bot.c:841`。
- `mimiclaw` 当前没有 Feishu 专用内置 PEM 证书，且 `tls_cert_bundle` 里明确是 “built-in fallback disabled”：`tuyaopen/apps/mimiclaw/tls_cert_bundle.c:95`。

补充说明：
- Tuya SDK 确实有“内置证书”，但这是 iotdns 服务自身（`h3/h4/h6.iot-dns.com`）的 CA，不是 Feishu/Telegram/Discord 业务域名 CA：`tuyaopen/src/tuya_cloud_service/cloud/tuya_register_center.c:24`、`:64`、`:68`。

### 7.4 结论

- 根因不是 `mimiclaw` 传参格式问题，也不是 Feishu 走了代码内置业务证书。
- 根因是 iotdns 后端对不同业务域名返回策略不同：对 Feishu 返回有效 `ca`，对 Telegram/Discord 返回空 `ca`，进而触发解析失败（`-9`）。

## 8. `mimiclaw/main` 与 `esp-idf` 实现对照

### 8.1 mimiclaw/main 的实现方式

#### 启动与初始化

- 入口：`mimi_app_main()` -> `user_main()` -> Linux `main()` / 非 Linux `tuya_app_main()`  
  `tuyaopen/apps/mimiclaw/mimi.c:345`、`:412`、`:422`、`:437`
- 运行时初始化里做了 `tuya_tls_init()` 与 `tuya_register_center_init()`：  
  `mimi.c:139-140`
- 网络就绪后统一启动在线服务（Telegram/Discord/Feishu）：  
  `mimi.c:389`、`:403`

#### 证书获取策略

- Telegram/Discord/Feishu 都是业务侧主动调 `mimi_tls_query_domain_certs()`：
  - Telegram：`tuyaopen/apps/mimiclaw/channels/telegram_bot.c:136`
  - Discord：`tuyaopen/apps/mimiclaw/channels/discord_bot.c:134`
  - Feishu：`tuyaopen/apps/mimiclaw/channels/feishu_bot.c:249`、`:841`
- `mimi_tls_query_domain_certs()` 内部调用 `tuya_iotdns_query_domain_certs()`，带重试退避；失败后没有内置业务证书回退：  
  `tuyaopen/apps/mimiclaw/tls_cert_bundle.c:72`、`:88`、`:95`

#### 一个关键实现差异（Tuya http_client）

- Tuya 的 `http_client_request` 里，`cacert == NULL` 会直接选择 `TRANSPORT_TYPE_TCP`（不是 TLS）：  
  `tuyaopen/src/libhttp/src/http_client_wrapper.c:86`
- 所以在 `mimiclaw` 里，若 iotdns 没拿到证书，某些 HTTP 路径不能通过“空证书”自动退化为“仍然 HTTPS”。

### 8.2 esp-idf 的实现方式

#### 应用层通常显式挂证书包

- 官方示例会在 TLS 配置里设置：`.crt_bundle_attach = esp_crt_bundle_attach`：  
  `esp-idf/examples/protocols/https_request/main/https_request_example_main.c:194-196`

#### 组件层证书选择顺序（HTTP/MQTT/ESP-TLS）

- `esp_http_client`：优先 `crt_bundle_attach`，其次 `use_global_ca_store`，再其次 `cert_pem`：  
  `esp-idf/components/esp_http_client/esp_http_client.c:827-835`
- `esp-tls (mbedtls)`：`crt_bundle_attach` 分支下强制 `MBEDTLS_SSL_VERIFY_REQUIRED`，并挂 bundle：  
  `esp-idf/components/esp-tls/esp_tls_mbedtls.c:945-950`
- `esp-mqtt`：同样支持 `use_global_ca_store` / `crt_bundle_attach` / 单独证书：  
  `esp-idf/components/mqtt/esp-mqtt/mqtt_client.c:142-147`

#### 证书包来源

- `esp_crt_bundle_attach()` 若用户未设置自定义 bundle，则使用 menuconfig 选择并嵌入固件的默认 bundle：  
  `esp-idf/components/mbedtls/esp_crt_bundle/include/esp_crt_bundle.h:23-24`  
  `esp-idf/components/mbedtls/esp_crt_bundle/esp_crt_bundle.c:333-339`

### 8.3 对照结论

- `mimiclaw`：业务域名证书优先靠 iotdns 动态返回；当 iotdns 失败时，回退到固件内置的 mini CA bundle（`ca-bundle-mini.pem`）。
- `esp-idf`：典型路径是“内置/全局证书包 + 严格校验”，不依赖 iotdns 才能建立对公网常见域名的 TLS 信任。
- 这也是两者在“iotdns 对某域名返回空 ca”场景下表现差异显著的根本原因。

### 8.4 回答：esp-idf 是否也是这么做

- 是。`esp-idf` 的常见实践就是：
  1. 优先使用证书包（`crt_bundle_attach`）或全局 CA store；
  2. 开启严格证书校验（`VERIFY_REQUIRED`）；
  3. 只有在明确业务需求时才使用单域名证书或弱化校验。
- 因此，前文“阶段2”本质是在 `mimiclaw` 里补一条更接近 esp-idf 的“全局信任根 + verify=true”能力，而不是继续扩大 no-verify 使用面。

## 9. iotdns 失败后的“第二条路”可行性

### 9.1 什么是 TLS no-verify

- `TLS no-verify` 指“连接仍使用 TLS 加密，但不校验证书链和域名”。
- 在实现上通常是 `verify=false`，即不校验服务端证书是否由受信任 CA 签发、也不严格校验证书与目标域名匹配。
- 这能提升故障场景下的可用性（证书缺失时仍可连通），但会降低安全性（存在中间人攻击风险）。

### 9.2 使用建议

- 将 no-verify 作为临时兜底机制，而不是长期默认配置。
- 优先恢复到 `verify=true`（例如 iotdns 成功返回证书，或接入全局 CA store）。
- 若必须启用 no-verify，建议限制域名白名单并记录告警/埋点，便于后续治理。

### 9.3 回答：全局信任根是否适用于所有域名

- 不是“对所有域名都无条件可用”，而是“对证书链可追溯到受信任根 CA 的域名可用”。
- 对公网常见站点（使用公开受信任 CA）通常可用。
- 对以下情况通常不适用或会失败：
  - 私有 CA / 自签证书；
  - 证书链不完整或过期；
  - 证书域名与访问域名不匹配（SNI/hostname 校验失败）；
  - 目标站点使用了证书包未收录的新根/中间 CA（需要更新 CA bundle）。

结论：全局信任根可以覆盖“绝大多数标准公网 TLS 域名”，但不是绝对覆盖全部域名。

### 9.4 第二条路分类

可以，但要区分两类“第二条路”：

1. **第二条路A：继续 TLS，但关闭证书校验（no-verify）**
   - 能最快恢复连通性，不依赖 iotdns 返回业务域名 `ca`。
   - 风险是无法校验服务端身份，存在中间人攻击风险。
   - 适合临时兜底，不适合长期默认策略。

2. **第二条路B：继续 TLS，并使用本地根证书包/系统 CA（类似 esp-idf）**
   - 安全性更好，可保持 `verify=true`。
   - 需要在 TuyaOpen 当前链路里补“全局 CA store / bundle attach”能力，属于架构增强，不是小改。

## 10. `tuyaopen/apps/mimiclaw` 是否可仿照 esp-idf

结论：**可以仿照，分阶段实现最稳妥**。

### 10.1 当前已落地（阶段1，先恢复可用）

- `libhttp` 请求结构新增 `tls_no_verify` 开关：  
  `tuyaopen/src/libhttp/include/http_client_interface.h:28`
- `http_client_request` 支持 `cacert==NULL` 但 `tls_no_verify=true` 时仍走 TLS：  
  `tuyaopen/src/libhttp/src/http_client_wrapper.c:86`、`:94-106`
- Telegram 在 iotdns 拿证书失败后切换 no-verify 回退：  
  `tuyaopen/apps/mimiclaw/channels/telegram_bot.c:131-151`、`:303`
- Discord 直连网关在 iotdns 失败后也切 no-verify 回退：  
  `tuyaopen/apps/mimiclaw/channels/discord_bot.c:134-139`、`:154-162`

### 10.2 后续可演进（阶段2/3，对齐 esp-idf 思路）

- 阶段2：引入“可选全局 CA store”，优先用全局 CA，iotdns 作为增量/覆盖。
- 阶段3：策略化校验：
  - 默认 `verify=true`。
  - 仅对白名单域名允许 no-verify 临时兜底。
  - 增加遥测与告警，统计 no-verify 命中率。

### 10.3 最新实现进展（已落地）

- `mimiclaw` 现在的证书回退顺序为：
  1. iotdns 证书查询（含重试退避）；
  2. 固件内置 mini CA bundle（`tuyaopen/apps/mimiclaw/certs/ca-bundle-mini.pem`，编译进固件）；
  3. 以上都失败后，才进入 no-verify 兜底（由上层 channel 逻辑触发）。
- `GLOBAL_CA` 的 SPIFFS/URL 路径已移除（不再依赖 `/spiffs/config/ca-bundle.pem`、不再下载全局 CA）。
- `llm_proxy`、`tool_web_search`、`tool_get_time`、`feishu` 直连 HTTP 路径已补齐 `.tls_no_verify` 传递，确保“无证书时仍走 TLS no-verify”。

说明（最新）：
- `mimiclaw` 的证书查询接口已改为 `size_t` 长度链路（`mimi_tls_query_domain_certs`）。
- iotdns 原生接口仍是 `uint16_t`，因此“iotdns 返回的单域证书”本身仍受其上游接口约束。
- 固件内置 mini bundle 不走 iotdns 长度参数，当前仓库默认文件约 `10.9KB`。

## 11. 本次问题的最终结论

- `open.feishu.cn` 可用，**不是因为代码内置了 Feishu 业务证书**，而是 iotdns 返回了非空 `ca`。
- `api.telegram.org`、`gateway.discord.gg` 失败，根因是 iotdns 对这些域名返回空 `ca`，触发 `-9`（JSON 取字段失败）。
- `mimiclaw` 已可以在 iotdns 失败时走“第二条路A（TLS no-verify）”保证连接；后续若要安全对齐 esp-idf，应继续做“第二条路B（全局 CA store）”。

## 12. 64KB 限制、证书包大小与默认小证书包

### 12.1 为什么之前看起来“只能 64KB”

- 之前 `mimiclaw` 证书长度使用 `uint16_t`，并在 `tls_cert_bundle.c` 有 `UINT16_MAX` 保护判断，所以 global CA bundle 实际被卡在 `<64KB`。
- 另外 Tuya 的 iotdns API（`tuya_iotdns_query_domain_certs`）本身参数就是 `uint16_t *cacert_len`，这条链路的返回长度也天然是 16 位。

### 12.2 现在是否可以放大

- iotdns 返回链路仍受 `uint16_t` 限制（上游接口限制）。
- 固件内置 CA bundle 可按需替换为更大的 PEM（通过替换 `ca-bundle-mini.pem` 并重新编译固件），不依赖 `MIMI_TLS_GLOBAL_CA_BUNDLE_MAX_BYTES`。
- 仍需注意：更大的 CA bundle 会增加 flash 占用、加载时内存占用和 TLS 握手解析开销。

### 12.3 默认小证书包（<60KB）

- 仓库已新增一个针对当前常见链路的精简包：  
  `tuyaopen/apps/mimiclaw/certs/ca-bundle-mini.pem`
- 当前文件大小约 `10.9KB`，包含 Telegram / Discord / Feishu 当前链路常见 CA（去重后）。
- 该文件会在构建时转换为固件内置常量并参与编译；运行时无需写入 `/spiffs/`。
