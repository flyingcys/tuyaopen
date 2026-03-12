# Tuya Cloud Service Host Tests Design

## Goal

为 `src/tuya_cloud_service` 建立一套分阶段推进的 Host 单元测试方案，优先覆盖低依赖、可稳定在本地 Linux 环境下运行的逻辑和边界模块，在不破坏现有组件结构的前提下逐步向主链路扩展。

## Current State

- `src/tuya_cloud_service` 目前没有已有的 `test/host` 目录。
- 组件内部模块数量多、交叉依赖重，既包含纯逻辑包装层，也包含明显偏集成的云连接主链路和平台适配逻辑。
- 当前仓库已经有可复用的 Host 测试基建：
  - `tests/host/CMakeLists.txt`
  - `tests/host/cmake/AddTuyaHostTest.cmake`
  - `tests/host/cmake/GenerateMocks.cmake`
  - `tests/host/support/stubs/`
- 新 worktree `feature/tuya-cloud-service-tests` 基线已验证：
  - 现有 Host suites `5/5` 通过
  - `tests/vendor/unity`、`tests/vendor/cmock`、`src/tal_kv/littlefs` 已初始化

## Design Principles

1. 先易后难，按依赖梯度推进，而不是一开始就尝试覆盖 `tuya_iot.c`、`mqtt_service.c`、`weather.c` 这类重依赖入口。
2. 继续沿用“组件内测试代码 + 根目录统一调度”的现有 Host 测试结构。
3. 优先测试：
   - 参数校验
   - 错误传播
   - 资源释放
   - KV/协议/transport 边界行为
4. 非必要不修改生产代码；只有在静态函数、硬编码依赖或全局状态完全阻塞测试时，才引入最小 test seam。
5. 所有新增行为都走 TDD：先写失败测试，再补最小实现或最小 seam。

## Test Layout

统一在组件根下建立：

- `src/tuya_cloud_service/test/host/CMakeLists.txt`
- `src/tuya_cloud_service/test/host/support/`
- `src/tuya_cloud_service/test/host/test_*.c`

不按子目录分散成多套 Host 入口，原因是：

- 该组件内部复用头文件和依赖很多，集中入口更容易共享 mocks、stubs 和 include 路径。
- 更符合当前仓库里 `src/<component>/test/host/` 的组织方式。
- 便于后续把 Phase 1 到 Phase 5 的测试逐步追加到同一组件测试资产中。

## Phase Plan

### Phase 1: Low-Dependency Entry Modules

首批实现模块：

- `cloud/tuya_endpoint.c`
- `transport/tuya_transport.c`
- `protocol/tuya_protocol.c`

选择原因：

- 依赖边界清晰，适合通过 CMock 和 stub 隔离外部调用。
- 业务价值明确，能建立后续 `tuya_cloud_service` 测试的 mock/stub 复用模式。
- 不要求拉起完整云连接状态机。

### Phase 2: Medium-Dependency Local Logic

候选模块：

- `cloud/tuya_health.c`
- `schema/tuya_iot_dp.c`

特点：

- 仍以本地状态管理和回调分发为主，但已经开始引入 work queue、client 全局状态和列表管理。
- 需要复用 Phase 1 的 mock 模式，再补线程/时间/workq 相关 stub。

### Phase 3: Service Wrappers

候选模块：

- `weather/tuya_weather.c`
- `cloud/tuya_http.c`
- `tls/tuya_tls.c` 的可独立包装层

特点：

- 对外部服务封装明显，但仍可在 Host 端通过 network/time/http/tls wrapper mock 控制行为。
- 风险在于依赖量增大，需要把测试范围限制在参数校验、请求构造和错误传播。

### Phase 4: Main Cloud Orchestrators

候选模块：

- `cloud/tuya_iot.c`
- `cloud/mqtt_service.c`
- `cloud/atop_service.c`
- `cloud/matop_service.c`

特点：

- 依赖密集、状态机复杂、全局 client/context 交织多。
- 更适合在已有 mocks/stubs 足够成熟后再进入。

### Phase 5: Peripheral / Integration-Oriented Modules

候选模块：

- `netmgr/*`
- `netcfg/*`
- `lan/*`
- `ble/*`
- `authorize/*`

特点：

- 更偏平台和集成路径，部分行为更适合最终走 Target/HIL 或更强集成测试。
- Host 单测只应保留明确可稳定验证的 contract 和状态转换。

## Phase 1 Test Scope

### `tuya_endpoint.c`

重点覆盖：

- `NULL` 参数保护
- KV 读写成功/失败路径
- `tuya_endpoint_init` 在读取后默认补全 `regist_key = "pro"` 的路径
- `tuya_endpoint_region_regist_set` 的内存内状态更新
- `tuya_endpoint_update` / `tuya_endpoint_update_auto_region` 在已有证书时先释放旧证书
- `tuya_endpoint_remove` 删除固定 key 的行为
- `tuya_endpoint_get` 返回内部缓存对象

依赖隔离：

- Mock `tal_kv_set/get/del/free`
- Mock `tal_kv_serialize_set/get`
- Mock `iotdns_cloud_endpoint_get`
- Stub `tal_log`

### `tuya_transport.c`

重点覆盖：

- transport array create/add/get/destroy
- 超过 `MAX_TRANSPORTER_NUM` 的边界
- scheme 命中与未命中
- `tuya_transporter_create` 对 TCP/TLS/WEBSOCKET 的分发
- read/write/connect/poll/close/ctrl wrapper 在函数指针存在/缺失时的行为
- `tuya_transporter_destroy` 对 `f_destroy` 的分发

依赖隔离：

- Mock `tal_malloc/free`
- Mock `mm_strdup`
- Fake transporter instances
- Mock `tuya_tcp_transporter_create`
- Mock `tuya_tls_transporter_create`
- 视编译宏情况决定是否 mock `tuya_websocket_transporter_create`

### `tuya_protocol.c`

重点覆盖：

- `tuya_pack_protocol_serial_no` 的首次随机种子与递增行为
- `tuya_parse_protocol_data` 的参数校验和 `DP_CMD_LAN` / `DP_CMD_MQ` 路由
- `tuya_pack_protocol_data` 的参数校验和分支选择
- 加解密 wrapper 失败时的错误传播
- LPV35 frame buffer size 计算
- `lpv35_frame_serialize` / `deserialize` 的参数校验与边界

依赖隔离：

- Mock `tal_malloc/free`
- Mock `tal_time_get_posix`
- Mock `uni_random_bytes/string/range`
- Mock `mbedtls_cipher_auth_encrypt_wrapper`
- Mock `mbedtls_cipher_auth_decrypt_wrapper`

## Production Code Change Policy

- 首先尝试通过现有公开函数和头文件完成测试。
- 如果某个分支只能通过静态函数间接触达，则优先通过公开 API 驱动，不直接暴露内部实现。
- 只有在以下情况才允许引入最小 seam：
  - 当前行为无法通过公开函数稳定验证
  - seam 不改变运行时行为
  - seam 仅用于替换硬编码依赖或暴露必要状态

## Verification Strategy

每个阶段都必须完成以下校验：

1. 组件测试单独构建并运行
2. 全量 Host suites 重新运行，确认无回归
3. 新增 mocks 生成正常
4. 文档中记录新增 suite 和依赖要求

## Recommended Rollout

推荐顺序：

1. Phase 1 只落 `tuya_endpoint`
2. Phase 1 再落 `tuya_transport`
3. Phase 1 最后落 `tuya_protocol`
4. 通过后再开始 Phase 2

原因：

- `tuya_endpoint` 最容易建立 KV mock 规范
- `tuya_transport` 能建立函数指针/transport fake 模式
- `tuya_protocol` 依赖最多，放在首批最后更稳

## Success Criteria

- `src/tuya_cloud_service/test/host/` 建立完成并接入 `tests/host/CMakeLists.txt`
- Phase 1 三个模块拥有可重复运行的 Host 单测
- 全量 Host 基线在新增测试后仍保持通过
- 后续 Phase 2-5 可以在同一测试入口上继续扩展，而不需要重构整体目录

## Implementation Status

- `2026-03-11`: Phase 1 first batch has been landed in the dedicated worktree.
- Current Host suites include:
  - `test_tuya_cloud_service_endpoint_host`
  - `test_tuya_cloud_service_transport_host`
  - `test_tuya_cloud_service_protocol_host`
  - `test_tuya_cloud_service_health_host`
  - `test_tuya_cloud_service_iot_dp_host`
  - `test_tuya_cloud_service_weather_host`
  - `test_tuya_cloud_service_http_host`
  - `test_tuya_cloud_service_tls_host`
  - `test_tuya_cloud_service_iot_host`
  - `test_tuya_cloud_service_mqtt_host`
  - `test_tuya_cloud_service_atop_host`
  - `test_tuya_cloud_service_matop_host`
  - `test_tuya_cloud_service_lan_host`
  - `test_tuya_cloud_service_authorize_host`
  - `test_tuya_cloud_service_netcfg_host`
  - `test_tuya_cloud_service_netmgr_host`
- Current full Host baseline: `21/21` suites passing.
