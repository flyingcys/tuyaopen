# TuyaOpen 单元测试最终方案

## 1. 结论

`tuyaopen` 适合采用一套以 `ESP-IDF` 为主参考、吸收 `FreeRTOS`、`lwIP`、`Mbed TLS` 长处的完整单元测试方案：

- **总体路线**：`Host 单元测试为主，Target/HIL 单元测试为校验`
- **架构原则**：`组件内就近放测试 + 根目录统一调度`
- **分层策略**：`TKL 以真实板卡测试为主，TAL/common/协议与中间件以 Host + Mock/Fake 为主`
- **工具组合**：`Unity + CMock + CTest/CMake + pytest + 覆盖率 + Sanitizer`
- **推进方式**：一次性定义完整目标，分阶段落地，不把当前未完成原型直接演变成正式基线

这比“只做 Host”更完整，也比“从第一天就强依赖真实硬件”更可落地，最符合 `tuyaopen` 这种跨芯片、跨平台、带明显 `TKL/TAL` 分层的 SDK。

## 1.1 当前落地进展（2026-03-11）

已完成：

- `tests/host` 基建已建立（`Unity`、`CMock`、`tuya_codegen`、基础 stubs）
- `src/common` Host 单测已接入并通过
- `src/tal_system` Host 单测已接入并通过
- `src/tal_kv` Host 单测已接入并通过
- `src/tal_network` Host 单测已接入并通过
- `src/tal_wifi` Host 单测已接入并通过
- Host 覆盖率脚本、本地执行脚本和 GitHub Actions 工作流已接入
- `test_app/unit_test_app` Target 单测应用骨架已接入并完成 `LINUX` 构建验证
- `tests/target/pytest` 调度骨架已接入，本地 `LINUX` smoke 结果为 `1 passed, 1 skipped`
- 开发者文档、组件模板和 PR 测试检查项已补齐

当前 Host 测试集结果：

- `test_common_host` PASS
- `test_tal_system_host` PASS
- `test_tal_kv_host` PASS
- `test_tal_network_host` PASS
- `test_tal_wifi_host` PASS

下一阶段：

- 在真实板卡上补 `tkl_gpio` 等首批 Target suite，并把 `flash_runner.py` 从 skeleton 扩展为可执行烧录流程

## 2. 参考项目分析后的取舍结论

### 2.1 ESP-IDF

应重点借鉴：

- 组件内就近维护测试代码
- `Host + Target` 双通路
- `pytest` 统一调度真实设备测试
- 测试工程和产品工程解耦

不应直接照搬：

- 过度绑定 `idf.py`、`TEST_CASE` 运行时和 ESP 专属标签体系
- 把所有目标测试都建立在单一芯片生态工具之上

原因：

`tuyaopen` 和 `ESP-IDF` 的相似点在于它们都不是单纯的 C 库，而是**面向多硬件平台的 SDK**。因此最终形态必须是“宿主机快速回归 + 真实板卡校验”的双路结构。但 `tuyaopen` 覆盖 T 系列、ESP32、Linux、BK、LN 等多类平台，不能把 Target 路径设计成某一家生态的专有模式。

### 2.2 FreeRTOS

应重点借鉴：

- `Unity + CMock` 在 C 项目里的高适配性
- `CTest/Make` 风格的宿主机单测执行方式
- 对核心模块额外引入形式化验证或静态验证的思路

不应直接照搬：

- 一开始就把 `CBMC` 等高成本验证纳入主流程

原因：

`tuyaopen` 当前连基础单测框架都未正式建立，第一目标应是先形成稳定、低摩擦、人人可运行的单测体系。形式化验证可以作为后续对 `tal_kv`、安全、协议解析等高风险模块的增强项，而不是落地前提。

### 2.3 lwIP

应重点借鉴：

- 按模块拆分测试套件
- 为测试预留轻量编译开关和 test hook
- 强调协议/状态机/边界条件覆盖，而不迷信 UI 式集成测试

不应直接照搬：

- 过于分散、偏底层库式的测试入口组织

原因：

`lwIP` 更像协议栈库；`tuyaopen` 是平台 SDK。前者的测试思想适合借鉴到 `tal_network`、`tal_wifi`、`tuya_cloud_service` 这些状态机和协议逻辑模块，但目录与入口不适合原样复用。

### 2.4 Mbed TLS

应重点借鉴：

- 数据驱动测试向量
- 侵入式可测试性设计
- 故障注入、内存/边界/配置组合测试
- Sanitizer 和安全相关测试分层

不应直接照搬：

- 过于复杂的测试数据生成链路
- 过早把大量兼容性矩阵纳入常规 PR 门禁

原因：

`tuyaopen` 的 `tal_security`、`tuya_cloud_service`、TLS/编码/解码相关逻辑，需要“黑盒 API 测试 + 白盒侵入式测试 + 标准向量测试”组合，而不是只靠调用成功/失败判断。

## 3. 对现有草稿的吸收与取舍

此前历史稿件：

- `tuyaopen_unit_test_scheme.md`
- `嵌入式单元测试分析报告(GLM46).md`
- `嵌入式单元测试方案分析.md`

本最终方案对其处理原则如下：

### 3.1 吸收的内容

- `Unity + CMock + pytest` 的总体方向
- `Host + Target` 双通路的基本判断
- `src/<component>/test` 组件内就近放测试的方向
- 先从 `common`、`tal_system`、`tal_kv`、`tal_network`、`tal_wifi` 切入的优先级判断

### 3.2 放弃的内容

- 与当前仓库结构不一致的 `components/unity`、`components/cmock` 假设
- 把未成型 demo 直接写成正式 CI 基线
- 把 mock 生成物、构建产物、占位脚本当成仓库资产

### 3.3 明确删除的原型内容

以下内容不应作为正式方案基线，因此已从当前工作区去掉：

- 未提交的 `tests/` 宿主机构建原型及其 `host_build` 产物
- 未提交的 `test_app/` 占位入口
- 未提交的 `src/common/test`、`src/tal_*/test` 原型测试目录
- 未提交的 `.github/workflows/unit_test.yml` 和 `pytest.ini` 占位版本

原因不是方向错误，而是**实现层级还不够稳定，继续沿用会把错误路径固化**。

## 4. TuyaOpen 最终测试分层模型

### 4.1 L0: Host 纯单元测试

对象：

- `src/common`
- `src/tal_system`
- `src/tal_kv`
- `src/tal_network`
- `src/tal_wifi`
- `src/tal_bluetooth`
- `src/tal_security` 中不依赖真实硬件的封装层
- `tuya_cloud_service` 中协议编码、解码、状态机、数据处理逻辑

特点：

- 在 Linux Host 上直接编译运行
- 使用 `CMock` Mock 掉 TKL、系统接口、外部依赖
- 提供覆盖率和 Sanitizer
- 作为 PR 级别的强制门禁

这是 `tuyaopen` 单元测试体系的**主战场**。

### 4.2 L1: Host 合约/组件边界测试

对象：

- `tal_kv` 与 `littlefs/flash` 的边界行为
- `tal_network`/`tal_wifi` 对底层网络接口的错误码映射
- `tuya_cloud_service` 与 TLS、MQTT、HTTP 封装层的边界行为

特点：

- 仍在 Host 运行
- 相比纯 Mock，更偏向 `Fake` 或测试双实现
- 检查“边界契约是否成立”，不是只测某个函数有没有被调用

这层是为了防止“Mock 测试全绿，但真实行为错位”。

### 4.3 L2: Target 单元测试 / HIL 测试

对象：

- `TKL` 适配层
- 板级驱动与寄存器行为
- 真实外设交互
- 必须依赖真实中断、时钟、DMA、Flash、Wi-Fi、BLE 的代码

特点：

- 编译为真实板卡固件
- 通过串口/JTAG/烧录器由 `pytest` 调度
- 核心目标是验证 `TKL` 的规范符合性和板级行为

这层是 `tuyaopen` 的**硬件真实性保障**，但不应承担主要回归负担。

### 4.4 L3: 非单元层验证

包括：

- 示例工程 smoke test
- Linux 目标完整运行验证
- 云连接/网络连通性集成测试

这层不是单元测试本体，但必须与单元测试体系协同；不能用它替代单元测试。

## 5. 最终目录方案

推荐目录结构如下：

```text
tuyaopen/
├── tests/
│   ├── host/
│   │   ├── CMakeLists.txt
│   │   ├── cmake/
│   │   ├── toolchains/
│   │   └── support/
│   │       ├── cmock/
│   │       │   └── cmock_config.yml
│   │       ├── fakes/
│   │       └── stubs/
│   ├── target/
│   │   ├── pytest/
│   │   │   ├── conftest.py
│   │   │   ├── test_tkl_gpio.py
│   │   │   └── test_tkl_flash.py
│   │   └── runners/
│   │       ├── serial_runner.py
│   │       └── flash_runner.py
│   └── vendor/
│       ├── unity/
│       └── cmock/
├── test_app/
│   └── unit_test_app/
│       ├── CMakeLists.txt
│       ├── app_default.config
│       └── src/
│           └── test_main.c
├── src/
│   ├── tal_wifi/
│   │   └── test/
│   │       ├── host/
│   │       │   ├── CMakeLists.txt
│   │       │   ├── test_tal_wifi_connect.c
│   │       │   └── mock_list.txt
│   │       └── target/
│   │           └── test_tkl_wifi_port.c
│   └── tal_kv/
│       └── test/
│           ├── host/
│           └── target/
└── tools/
    └── test/
        ├── gen_mocks.py
        └── run_target_tests.py
```

关键原则：

- **测试代码仍然属于组件资产**，因此放在 `src/<component>/test/`
- **宿主机构建与运行入口统一放在 `tests/host/`**
- **真实板卡测试调度统一放在 `tests/target/`**
- **Target 固件入口统一放在 `test_app/unit_test_app/`**

## 6. 构建与执行方案

### 6.1 Host 路径

Host 测试不应强耦合根目录产品构建系统，而应独立成一个原生 CMake 入口。

推荐命令：

```bash
cmake -S tests/host -B build/tests/host -DTUYA_ENABLE_COVERAGE=ON
cmake --build build/tests/host -j
ctest --test-dir build/tests/host --output-on-failure
```

可选增强：

```bash
cmake -S tests/host -B build/tests/host-asan -DTUYA_ENABLE_ASAN=ON -DTUYA_ENABLE_UBSAN=ON
cmake --build build/tests/host-asan -j
ctest --test-dir build/tests/host-asan --output-on-failure
```

设计原因：

- 当前根 `CMakeLists.txt` 强绑定平台与产品工程
- Host 单测若依附产品构建，会被平台配置、Kconfig、工具链和 example 构建拖慢
- 独立入口更像 `FreeRTOS/coreMQTT/coreHTTP` 的宿主机单测思路，也更适合 CI

### 6.2 Target 路径

Target 单测需要是一个真正的 `tuyaopen` 应用，通过现有 `tos.py build` 工作流产出固件。

推荐命令形态：

```bash
cd /workspace && . ./export.sh
mkdir -p .cache && touch .cache/.dont_prompt_update_platform
cd test_app/unit_test_app
tos.py check
tos.py build
pytest ../../tests/target/pytest -m target
```

注意：

- `pytest` 是**总调度器**
- `pytest-embedded` 只作为可选适配器，不应成为所有平台唯一依赖
- 对 ESP32/Linux 等适合的目标可直接接入 `pytest-embedded`
- 对 T 系列、BK、LN 等平台应通过 `tests/target/runners/` 里的通用烧录/串口抽象接入

结论：

`pytest` 要学 `ESP-IDF`，但 **`pytest-embedded` 不能被设计成 TuyaOpen 全平台唯一基石**。

## 7. 工具链最终选型

### 7.1 必选

- **Unity**
  - C 语言嵌入式项目事实标准之一
  - 轻量、易移植
  - 同时适配 Host 和 Target

- **CMock**
  - 自动生成 C 接口 Mock
  - 对 `TKL -> TAL`、`TAL -> middleware` 边界非常适合

- **CMake + CTest**
  - 负责 Host 构建、分 suite 注册和本地/CI 执行

- **pytest**
  - 负责 Target/HIL 调度、串口日志解析、固件刷写编排

### 7.2 强烈建议

- **gcov/lcov 或 gcovr**
  - 覆盖率统计

- **AddressSanitizer / UndefinedBehaviorSanitizer**
  - 宿主机内存与未定义行为问题检测

### 7.3 后续增强

- **CBMC 或等价形式化验证**
  - 仅用于高风险核心模块

- **故障注入工具**
  - 用于模拟内存分配失败、flash 异常、随机数异常、超时等

版本管理建议：

- `tests/vendor/unity` 和 `tests/vendor/cmock` 建议以 **git submodule 固定版本**
- 不建议继续引用仓库内第三方子模块自带的 `unity` 副本，避免路径偶合和版本漂移

## 8. Mock、Stub、Fake 与测试向量策略

### 8.1 Mock

适用场景：

- `tal_wifi` Mock `tkl_wifi`
- `tal_network` Mock `tkl_network`
- `tal_kv` Mock `tkl_flash`、互斥、加密接口
- `tal_system` Mock `tkl_system`、`tkl_memory`

原则：

- 只 Mock **边界依赖**
- 不 Mock 被测模块内部实现
- 自动生成的 mock 代码不入库，只进构建目录

### 8.2 Stub

适用场景：

- 日志函数
- 时间戳、随机数、弱符号回调
- 少量简单外部函数占位

原则：

- 只处理简单返回和最小依赖
- 不承载复杂行为

### 8.3 Fake

适用场景：

- 文件系统、KV 存储、网络事件源
- 需要保留基本行为但不想上真实硬件时

原则：

- 当仅用 Mock 会让测试失真时，优先使用 Fake
- `tal_kv`、`tuya_cloud_service` 的部分测试更适合 Fake 而不是纯 Mock

### 8.4 测试向量

必须用于：

- `tal_security`
- TLS 编解码
- Base64/Hash/AES/SHA/RSA/ECC 封装
- 云协议编解码

原则：

- 优先使用标准公开测试向量
- 测试数据与测试逻辑分离
- 允许像 `Mbed TLS` 一样对同一 API 做多配置、多向量组合测试

## 9. 可测试性设计规范

这是方案能否真正落地的关键。

### 9.1 内部符号可测试

参考 `Mbed TLS` 的 invasive testing 思路，建议引入：

```c
#ifdef TUYA_TEST_HOOKS
#define TUYA_STATIC_TESTABLE
#else
#define TUYA_STATIC_TESTABLE static
#endif
```

适用场景：

- 复杂静态函数确有必要白盒测试
- 不应为测试把内部实现暴露进 public header

### 9.2 测试专用内部头文件

允许为复杂组件增加内部测试头，例如：

```text
src/tal_kv/src/tal_kv_internal.h
src/tuya_cloud_service/src/cloud_proto_internal.h
```

规则：

- 仅供本组件源码和测试使用
- 不进入对外安装头文件

### 9.3 全局状态可复位

要求：

- 有全局状态的组件必须提供 `reset_for_test()` 或等价重置入口
- `setUp/tearDown` 后不能遗留全局污染

### 9.4 时间、随机数、内存失败可注入

要求：

- 时间相关逻辑必须允许注入时钟源
- 随机数调用必须可被 stub/fake
- 关键路径应支持内存分配失败注入

这部分是 `lwIP + Mbed TLS` 经验在 `tuyaopen` 中最值得引入的内容。

## 10. 覆盖率与质量门禁

### 10.1 覆盖率边界

覆盖率只统计 **Host 单元测试**，不统计 Target/HIL。

原因：

- Target 测试更偏硬件行为验证
- 跨平台统计难度高且不稳定
- Host 覆盖率最适合做持续门禁

### 10.2 阶段性目标

Phase 1：

- 首批试点组件行覆盖率 `>= 70%`
- 首批试点组件分支覆盖率 `>= 50%`

Phase 2：

- 核心组件行覆盖率 `>= 80%`
- 核心组件分支覆盖率 `>= 60%`

长期目标：

- 关键组件新增/修改代码的变更覆盖率 `>= 80%`

### 10.3 PR 门禁

PR 必须通过：

- 格式检查
- Host 编译
- Host 单元测试
- 至少一条 Sanitizer lane
- 变更组件覆盖率检查

Nightly 才运行：

- 多板卡 HIL
- Wi-Fi/BLE/Flash 等真实硬件单测
- 长时间稳定性和异常恢复测试

## 11. CI/CD 最终方案

### 11.1 PR 级

执行内容：

- `python tools/check_format.py --base <branch>`
- `cmake -S tests/host -B build/tests/host`
- `ctest --test-dir build/tests/host --output-on-failure`
- 覆盖率报告生成
- `ASAN/UBSAN` lane

原则：

- 快
- 稳
- 无板卡依赖

### 11.2 Nightly 级

执行内容：

- 构建 `test_app/unit_test_app`
- 对选定板卡刷机
- 运行 `tests/target/pytest`
- 汇总 PASS/FAIL、串口日志、固件版本、板卡型号

建议首批板卡：

- `LINUX`
- 一个 Tuya T 系列主平台
- 一个非 Tuya 芯片平台，例如 `ESP32` 或 `BK7231N`

### 11.3 Release 级

执行内容：

- 扩展板卡矩阵
- 关键组件完整 HIL 回归
- 覆盖率归档
- 产出测试摘要报告

## 12. 组件优先级与首批落地范围

### 12.1 Phase 0：基建

- 建立 `tests/host`
- 引入 `Unity/CMock`
- 建立 mock 生成规则
- 建立覆盖率和 Sanitizer 规则
- 建立 `test_app/unit_test_app`
- 建立 `tests/target/pytest` 基础 runner

### 12.2 Phase 1：首批 Host 试点

建议只做以下组件：

- `src/common`
- `src/tal_system`
- `src/tal_kv`
- `src/tal_network`

原因：

- 价值高
- 与硬件解耦程度相对更高
- 最容易沉淀出统一写法

### 12.3 Phase 2：扩展 Host

- `src/tal_wifi`
- `src/tal_bluetooth`
- `src/tal_security`
- `src/tuya_cloud_service` 中协议与编解码部分

### 12.4 Phase 3：Target/HIL

- `tools/porting/adapter/gpio/tkl_gpio.h`
- `tools/porting/adapter/flash/tkl_flash.h`
- `tools/porting/adapter/uart/tkl_uart.h`
- `tools/porting/adapter/timer/tkl_timer.h`
- `tools/porting/adapter/wifi/tkl_wifi.h`
- `tools/porting/adapter/bluetooth/tkl_bluetooth.h`

### 12.5 Phase 4：高级增强

- 安全测试向量体系
- 故障注入体系
- 关键模块形式化验证

## 13. 不建议采用的方案

### 13.1 不建议把所有测试都放在根 `tests/`，完全脱离组件目录

问题：

- 组件维护者无法就近维护测试
- 测试与代码演化脱节

### 13.2 不建议一开始就把所有 TKL 测试放到 Host 里做

问题：

- TKL 的核心价值就是对真实芯片和外设行为做适配
- 纯 Host Mock 不能证明端口正确

### 13.3 不建议直接把 `pytest-embedded` 当成全平台唯一方案

问题：

- `tuyaopen` 不是单一芯片框架
- 多平台烧录、串口、复位方式不统一

### 13.4 不建议把自动生成 mock、构建目录、覆盖率文件提交到仓库

问题：

- 噪音大
- 易冲突
- 没有长期维护价值

### 13.5 不建议把“示例工程能跑”当成单元测试通过

问题：

- 示例验证的是系统集成，不是边界条件和异常路径
- 无法覆盖大量错误分支

## 14. 对 TuyaOpen 的最终推荐

最终推荐方案可以概括成一句话：

> **以 `ESP-IDF` 风格的组件化双通路测试体系为主框架，以 `FreeRTOS` 的 Unity/CMock 宿主机实践为落地基础，以 `lwIP` 的模块化与 test hook 思想提升可测性，以 `Mbed TLS` 的测试向量和侵入式测试规范覆盖安全与复杂内部逻辑。**

具体落地决策如下：

- **正式方案采用完整方案，不采用最小方案**
- **实施顺序采用分阶段推进，不追求一次全部完成**
- **主基线建立在 Host，不建立在真实板卡**
- **Target/HIL 是必要组成部分，但属于第二主线**
- **目录采用组件内就近测试 + 根目录统一调度**
- **当前旧原型不继续沿用，按本方案重建**

这是一套对 `tuyaopen` 当前状态、未来演进和多平台属性都更稳妥的最终技术方案。

## 15. 参考资料

官方资料：

- ESP-IDF Unit Testing Guide: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/unit-tests.html>
- ESP-IDF Linux Host Apps / Host Build 说明: <https://docs.espressif.com/projects/esp-idf/en/v5.0.5/esp32/api-guides/host-apps.html>
- FreeRTOS coreMQTT README: <https://github.com/FreeRTOS/coreMQTT>
- FreeRTOS coreHTTP README: <https://github.com/FreeRTOS/coreHTTP>
- lwIP 官方仓库: <https://github.com/lwip-tcpip/lwip>
- Mbed TLS Getting Started: <https://github.com/Mbed-TLS/mbedtls>
- Mbed TLS Testing Architecture: <https://mbed-tls.readthedocs.io/en/latest/kb/development/test-framework/>
- Mbed TLS Invasive Testing: <https://mbed-tls.readthedocs.io/en/latest/kb/development/invasive-testing/>

本次汇总吸收的历史稿件：

- `../tuyaopen_unit_test_scheme.md`
- `../嵌入式单元测试分析报告(GLM46).md`
- `../嵌入式单元测试方案分析.md`
