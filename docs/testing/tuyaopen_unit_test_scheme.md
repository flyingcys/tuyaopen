# tuyaopen 单元测试方案

基于对 `esp-idf`、`FreeRTOS`、`lwip` 和 `mbedtls` 等主流嵌入式开源项目单元测试方案的深度分析，结合 `tuyaopen` 作为硬件芯片平台框架的特性（高度类似 `esp-idf`，具有明显的硬件抽象层和组件化特征），为其量身定制了以下单元测试方案。

## 1. 核心设计理念

1. **组件化测试 (Component-based)**：测试代码与业务代码就近存放，每个组件维护自己的测试用例，降低耦合，提高可维护性。
2. **双端执行 (Dual-Target)**：
   - **Host 端 (Linux/macOS/Windows)**：利用 Mock 技术隔离硬件，实现毫秒级快速验证，便于 CI/CD 和 TDD 开发，支持代码覆盖率统计。
   - **Target 端 (真实芯片/QEMU)**：在真实硬件上运行相同的测试用例，确保底层驱动和真实环境的正确性。
3. **分层测试策略 (TKL vs TAL)**：针对 TuyaOS 的架构（TKL 硬件内核层，TAL 抽象层），采取不同的 Mock 和执行策略，精准打击。

## 2. 工具链选型

为了实现上述设计理念，推荐采用以下成熟且广泛使用的开源工具链：

- **测试框架：Unity**
  - **优势**：C 语言原生，极轻量，支持丰富的断言宏，适合资源受限的嵌入式环境。ESP-IDF 和 FreeRTOS 均采用此框架。
- **Mock 工具：CMock**
  - **优势**：与 Unity 无缝集成，可通过解析 C 语言头文件自动生成 Mock 函数，极大简化硬件依赖的解耦工作。
- **自动化执行：Pytest + pytest-embedded**
  - **优势**：参考 ESP-IDF 的成功经验，使用 Python 编写测试执行脚本。`pytest-embedded` 能够方便地在 Host 端控制真实开发板的烧录、复位，并抓取串口日志进行断言。
- **覆盖率工具：gcov + lcov + gcovr**
  - **优势**：GCC 内置支持，配合 lcov/gcovr 可生成直观的 HTML 覆盖率报告，用于 Host 端测试的覆盖率统计和 CI 门禁。

## 3. 目录组织结构

推荐采用与 `esp-idf` 类似的组件内部就近组织方式，使得测试代码成为组件资产的一部分：

```text
tuyaopen/
├── components/
│   ├── tal_wifi/
│   │   ├── src/
│   │   │   └── tal_wifi.c
│   │   ├── include/
│   │   │   └── tal_wifi.h
│   │   └── test/                 # 组件级测试目录
│   │       ├── test_tal_wifi.c   # 测试用例
│   │       └── CMakeLists.txt    # 测试构建脚本
│   └── tkl_gpio/
│       ├── src/
│       ├── include/
│       └── test/
├── test_app/                       # 统一的测试工程入口 (类似 esp-idf 的 test_apps)
│   ├── main/
│   │   └── test_main.c           # Unity 运行器入口，负责注册和调度各组件测试
│   └── CMakeLists.txt
└── pytest.ini                      # Pytest 配置文件
```

## 4. 针对 tuyaopen 分层架构的测试策略

`tuyaopen` 具有清晰的分层架构，主要分为 TKL（Tuya Kernel Layer，硬件内核层）和 TAL（Tuya Abstraction Layer，抽象层）及各类中间件。针对不同层级，测试策略应有所侧重：

### 4.1 TKL 层 (Tuya Kernel Layer)
- **特性**：与底层芯片硬件强相关，负责实现具体的硬件驱动（如 GPIO、I2C、SPI 等）。
- **测试策略**：**直接在 Target (真实芯片) 上运行，不使用 Mock。**
- **测试目的**：验证芯片原厂对接的 TKL 接口是否符合 Tuya 规范，确保硬件行为的正确性。
- **示例**：测试 `tkl_gpio_write` 时，通过万用表或回环连接（Loopback）验证引脚电平变化。

### 4.2 TAL 层 (Tuya Abstraction Layer) 及中间件
- **特性**：业务逻辑为主，对下调用 TKL 接口，对上提供统一的 API。
- **测试策略**：**主要在 Host 端运行，使用 CMock 对底层的 TKL 接口进行 Mock。**
- **测试目的**：脱离硬件环境，快速验证状态机、协议解析、内存管理等纯软件逻辑，覆盖各种异常分支。
- **示例**：测试 `tal_wifi_connect` 时，Mock 底层的 `tkl_wifi_init` 等接口，验证在不同返回值下的重试逻辑和状态流转。

## 5. 构建与执行流

基于 CMake 构建系统，实现 Host 端和 Target 端的双流转：

### 5.1 Host 端测试流 (快速反馈 & 覆盖率)
1. **配置构建**：`cmake -DTARGET=host -DENABLE_COVERAGE=ON`
2. **编译**：编译 Host 版本的可执行文件，CMock 在此阶段自动生成 Mock 代码。
3. **执行**：直接运行 `./test_app`。
4. **覆盖率**：使用 `make coverage` 生成 `lcov` 覆盖率 HTML 报告。

### 5.2 Target 端测试流 (真实硬件验证)
1. **配置构建**：`cmake -DTARGET=chip_platform` (如 `bk7231n` 等)。
2. **交叉编译**：生成目标平台的固件（`.bin` / `.elf`）。
3. **自动化测试**：
   - 运行 `pytest --target chip_platform`。
   - `pytest-embedded` 自动将固件烧录到开发板。
   - 监控串口输出，触发 Unity 测试（如发送特定命令或复位）。
   - 解析串口打印的 Unity 测试结果（PASS/FAIL/IGNORE）。

## 6. CI/CD 落地路线图

为了平稳推进单元测试的落地，建议分三个阶段实施：

### Phase 1：基础建设与 Host 端验证 (1-2个月)
- 引入 Unity 和 CMock 到构建系统。
- 搭建 `test_app` 测试工程框架。
- 选取 1-2 个核心 TAL 组件（如 `tal_system` 或 `tal_memory`）编写 Host 端测试用例。
- 跑通 Host 端测试流，生成代码覆盖率报告。

### Phase 2：Target 端自动化与 TKL 验证 (2-3个月)
- 引入 `pytest` 和 `pytest-embedded` 框架。
- 搭建基础的硬件测试环境（HIL）。
- 选取核心 TKL 组件（如 `tkl_gpio`、`tkl_uart`）编写 Target 端测试用例。
- 实现固件的自动烧录和测试结果的自动解析。

### Phase 3：CI/CD 深度集成与推广 (3-6个月)
- 集成到 GitHub Actions 或 GitLab CI。
- **PR 门禁**：每次提交自动触发 Host 端测试，检查代码覆盖率是否达标。
- **Nightly Build**：每晚定时触发 Target 端（HIL）测试，验证在真实硬件上的稳定性。
- 制定团队测试规范，要求新增代码必须包含对应的单元测试。

## 7. 落地行动项与执行追踪 (Action Items Tracking)

以下是根据方案拆解的具体行动项。我们将按照优先级逐步实现，并在完成后更新状态：

- [x] **Action 1: 初始化基础目录与 Unity 框架 (优先级: 高)**
  - 创建 `tuyaopen` 目录结构。
  - 引入 Unity 测试框架源码作为基础组件。
- [x] **Action 2: 创建示例组件 (TKL 与 TAL) (优先级: 高)**
  - 创建 `tkl_gpio` 接口定义。
  - 创建 `tal_wifi` 组件及其依赖逻辑。
- [x] **Action 3: 构建 Host 端测试工程与 CMake 体系 (优先级: 高)**
  - 编写 `test_app/main/test_main.c`。
  - 编写 `tal_wifi` 的单元测试用例。
  - 配置支持代码覆盖率的 CMakeLists.txt。
- [x] **Action 4: 搭建 Target 端自动化测试脚本 (优先级: 中)**
  - 配置 `pytest.ini`。
  - 编写基于 `pytest-embedded` 的自动化执行脚本示例。
- [x] **Action 5: 配置 CI/CD 流水线 (优先级: 中)**
  - 编写 GitHub Actions 工作流文件，实现自动化触发。

## 8. tuyaopen `src` 目录组件单元测试覆盖计划

基于 `tuyaopen/src` 目录下的实际组件结构，我们将分批次、按优先级逐步完成核心组件的单元测试覆盖。以下是具体的行动项（To-Do List）：

### Phase 1: 基础设施与系统核心组件 (TAL System & Utils)
- [ ] **Action 1.1: `tal_system` 组件测试**
  - 覆盖内存管理、线程、互斥锁、信号量等基础系统抽象接口。
  - Mock 底层 RTOS/TKL 接口，验证内存泄漏检测、超时处理等逻辑。
- [ ] **Action 1.2: `tal_kv` (Key-Value 存储) 组件测试**
  - 覆盖键值对的读写、删除、加密存储逻辑。
  - Mock 底层 Flash 读写接口，验证在存储满、读写失败等异常情况下的鲁棒性。
- [ ] **Action 1.3: `common` 目录工具类测试**
  - 覆盖链表、队列、环形缓冲区、字符串处理等通用数据结构和算法。
  - 纯逻辑测试，无需复杂 Mock。

### Phase 2: 通信与网络抽象组件 (TAL Network & Connectivity)
- [ ] **Action 2.1: `tal_wifi` 组件测试**
  - 覆盖 Wi-Fi 初始化、扫描、连接、断开等状态机流转。
  - Mock `tkl_wifi` 接口，验证重连机制和事件回调。
- [ ] **Action 2.2: `tal_network` (Socket 抽象) 组件测试**
  - 覆盖 TCP/UDP 创建、连接、收发数据逻辑。
  - Mock 底层 LwIP 或 TKL 网络接口，验证非阻塞模式、超时和错误码转换。
- [ ] **Action 2.3: `tal_bluetooth` 组件测试**
  - 覆盖 BLE 广播、扫描、连接、GATT 服务注册与数据收发。
  - Mock `tkl_bluetooth` 接口。

### Phase 3: 外设驱动抽象组件 (TAL Driver)
- [ ] **Action 3.1: `tal_driver` 基础外设测试 (GPIO, UART, Timer)**
  - 覆盖外设的初始化、配置、读写和中断回调注册。
  - Mock 对应的 `tkl_gpio`, `tkl_uart`, `tkl_timer` 等接口。
- [ ] **Action 3.2: `tal_driver` 高级外设测试 (I2C, SPI, PWM)**
  - 覆盖总线通信协议的抽象封装逻辑。

### Phase 4: 涂鸦核心服务与协议 (Tuya Services)
- [ ] **Action 4.1: `tuya_cloud_service` (云端服务) 测试**
  - 覆盖设备激活、MQTT 连接、DP 点数据上报与下发解析。
  - Mock 网络层和加密层，验证协议组包和解包的正确性。
- [ ] **Action 4.2: `tal_security` (安全与加密) 组件测试**
  - 覆盖 AES, RSA, SHA, ECC 等加密算法的封装。
  - 使用标准测试向量（Test Vectors）验证加密结果的正确性。
