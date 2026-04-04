# xiaozhi 注册链路修复实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 `apps/xiaozhi` 当前注册链路中的编译阻塞、设备身份漂移、MQTT 下行订阅错误和激活配置缺口，并完成最小可验证闭环。

**Architecture:** 先把可独立验证的纯逻辑提取为小函数并补单测，再做最小实现修改，最后分别用主机侧逻辑测试和目标侧构建验证收敛。避免把网络、KV、CLI、设备标识生成耦在一起，优先让“身份生成”和“MQTT topic 推导”可脱离 Tuya 运行时单独测试。

**Tech Stack:** C99、TuyaOpen TAL/KV/CLI、`cc` 单文件单测、`tos.py build`

---

### Task 1: 建立纯逻辑测试入口

**Files:**
- Create: `apps/xiaozhi/tests/test_xiaozhi_identity.c`
- Create: `apps/xiaozhi/tests/test_xiaozhi_mqtt_topic.c`
- Modify: `apps/xiaozhi/src/xiaozhi_system.c`
- Modify: `apps/xiaozhi/src/xiaozhi_mqtt_udp.c`

- [ ] **Step 1: 写失败测试，覆盖设备 ID 持久化回退和 MQTT 订阅 topic 推导**

```c
/*
 * 断言：
 * 1. subscribe_topic 为空或为 "null" 时，使用 publish_topic 前缀 + /p2p/GID_test@@@<mac下划线格式>
 * 2. mac 非法或 publish_topic 为空时返回失败
 * 3. 设备 MAC 获取失败时，回退值可被持久化并复用
 */
```

- [ ] **Step 2: 运行测试并确认失败**

Run: `cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_mqtt_topic.c -o /tmp/test_xz_mqtt_topic && /tmp/test_xz_mqtt_topic`
Expected: FAIL，提示目标函数未定义或行为不符

Run: `cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_identity.c -o /tmp/test_xz_identity && /tmp/test_xz_identity`
Expected: FAIL，提示目标函数未定义或行为不符

- [ ] **Step 3: 实现最小纯逻辑函数**

```c
/*
 * 新增/暴露的小函数：
 * - xz_build_device_subscribe_topic(...)
 * - xz_format_or_reuse_fallback_device_id(...)
 */
```

- [ ] **Step 4: 重新运行测试并确认通过**

Run: `cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_mqtt_topic.c -o /tmp/test_xz_mqtt_topic && /tmp/test_xz_mqtt_topic`
Expected: PASS

Run: `cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_identity.c -o /tmp/test_xz_identity && /tmp/test_xz_identity`
Expected: PASS

- [ ] **Step 5: 提交一个最小变更点**

```bash
git add apps/xiaozhi/tests/test_xiaozhi_identity.c apps/xiaozhi/tests/test_xiaozhi_mqtt_topic.c apps/xiaozhi/src/xiaozhi_system.c apps/xiaozhi/src/xiaozhi_mqtt_udp.c
git commit -m "fix: cover xiaozhi identity and mqtt topic logic"
```

### Task 2: 修复 KV 写入编译阻塞并接入设备身份持久化

**Files:**
- Modify: `apps/xiaozhi/src/xiaozhi_settings.c`
- Modify: `apps/xiaozhi/src/xiaozhi_system.c`
- Test: `apps/xiaozhi/tests/test_xiaozhi_identity.c`

- [ ] **Step 1: 写失败测试，覆盖 fallback 设备 ID 复用**

```c
/*
 * 断言多次调用在已有持久化值时返回相同 device_id
 */
```

- [ ] **Step 2: 运行测试并确认失败**

Run: `cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_identity.c -o /tmp/test_xz_identity && /tmp/test_xz_identity`
Expected: FAIL

- [ ] **Step 3: 写最小实现**

```c
/*
 * 1. tal_kv_set 参数改为 const uint8_t *
 * 2. xiaozhi_system_get_device_id 在真实 MAC 获取失败时先读 KV，
 *    没有则生成并持久化 fallback 值
 */
```

- [ ] **Step 4: 跑测试确认通过**

Run: `cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_identity.c -o /tmp/test_xz_identity && /tmp/test_xz_identity`
Expected: PASS

- [ ] **Step 5: 验证 T5AI 编译不再卡在 KV 签名错误**

Run: `bash -lc 'rm -rf apps/xiaozhi/.build apps/xiaozhi/dist && . ./export.sh >/tmp/xz_export.log && cd apps/xiaozhi && tos.py build'`
Expected: 不再出现 `xiaozhi_settings.c:35` 的 `pointer-sign` 错误

### Task 3: 修复 MQTT 订阅 topic 推导

**Files:**
- Modify: `apps/xiaozhi/src/xiaozhi_mqtt_udp.c`
- Modify: `apps/xiaozhi/src/xiaozhi_app.c`
- Test: `apps/xiaozhi/tests/test_xiaozhi_mqtt_topic.c`

- [ ] **Step 1: 写失败测试，覆盖 `"null"` / 空串场景**

```c
/*
 * 输入 publish_topic=device-server, subscribe_topic="null", mac=aa:bb:cc:dd:ee:ff
 * 输出 device-server/p2p/GID_test@@@aa_bb_cc_dd_ee_ff
 */
```

- [ ] **Step 2: 运行测试并确认失败**

Run: `cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_mqtt_topic.c -o /tmp/test_xz_mqtt_topic && /tmp/test_xz_mqtt_topic`
Expected: FAIL

- [ ] **Step 3: 写最小实现**

```c
/*
 * 连接前统一调用 topic 规范化逻辑：
 * - subscribe_topic 为空或 "null" 时，按 py-xiaozhi 规则推导
 * - 显式配置了有效 topic 时保留原值
 */
```

- [ ] **Step 4: 重新跑测试确认通过**

Run: `cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_mqtt_topic.c -o /tmp/test_xz_mqtt_topic && /tmp/test_xz_mqtt_topic`
Expected: PASS

- [ ] **Step 5: 用真实 OTA 返回样本做一次静态核对**

Run: `python3 - <<'PY'\nprint('device-server/p2p/GID_test@@@02_11_22_33_44_55')\nPY`
Expected: 输出的 topic 格式与 `py-xiaozhi` 规则一致

### Task 4: 补激活配置入口与文档

**Files:**
- Modify: `apps/xiaozhi/src/cli_cmd.c`
- Modify: `apps/xiaozhi/README_CN.md`
- Modify: `apps/xiaozhi/README.md`

- [ ] **Step 1: 写失败测试或最小可执行验证目标**

```c
/*
 * 这里不强行做 CLI 单测，改为通过命令帮助和 show 输出验证：
 * 需要新增 serial_number / activation_secret 的 set/show 入口
 */
```

- [ ] **Step 2: 先实现失败前提的手工检查**

Run: `rg -n "serial_number|activation_secret" apps/xiaozhi/src/cli_cmd.c apps/xiaozhi/README_CN.md apps/xiaozhi/README.md`
Expected: 当前缺失或不完整

- [ ] **Step 3: 写最小实现**

```c
/*
 * 新增例如：
 * - xz_sys show
 * - xz_sys serial_number <value>
 * - xz_sys activation_secret <value>
 */
```

- [ ] **Step 4: 重新检查帮助与文档**

Run: `rg -n "serial_number|activation_secret|xz_sys" apps/xiaozhi/src/cli_cmd.c apps/xiaozhi/README_CN.md apps/xiaozhi/README.md`
Expected: 三处都能看到入口和说明

- [ ] **Step 5: 提交文档和 CLI 变更**

```bash
git add apps/xiaozhi/src/cli_cmd.c apps/xiaozhi/README_CN.md apps/xiaozhi/README.md
git commit -m "feat: add xiaozhi activation config commands"
```

### Task 5: 最小闭环验证

**Files:**
- Modify: `apps/xiaozhi/app_default.config`（仅当需要切 Linux 默认验证）
- Modify: `apps/xiaozhi/config/Linux.config`（仅当需要补最小验证开关）

- [ ] **Step 1: 运行纯逻辑测试**

Run: `cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_mqtt_topic.c -o /tmp/test_xz_mqtt_topic && /tmp/test_xz_mqtt_topic`
Expected: PASS

Run: `cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_identity.c -o /tmp/test_xz_identity && /tmp/test_xz_identity`
Expected: PASS

- [ ] **Step 2: 运行目标构建**

Run: `bash -lc 'rm -rf apps/xiaozhi/.build apps/xiaozhi/dist && . ./export.sh >/tmp/xz_export.log && cd apps/xiaozhi && tos.py build'`
Expected: 至少越过已知 `xiaozhi_settings.c` 编译阻塞；若有新错误，记录为下一层问题

- [ ] **Step 3: 运行 OTA 样本验证**

Run: `python3 - <<'PY'\nimport json\nsample={\"endpoint\":\"mqtt.xiaozhi.me\",\"client_id\":\"GID_test@@@02_11_22_33_44_55@@@uuid\",\"publish_topic\":\"device-server\",\"subscribe_topic\":\"null\"}\nprint(json.dumps(sample, ensure_ascii=False))\nPY`
Expected: 后续人工核对代码逻辑已能从该输入推导出真实订阅 topic

- [ ] **Step 4: 汇总残余风险**

```text
如果仍有 T5AI 目标新增编译错误，单独列为“构建兼容性剩余项”，不混入注册链路逻辑缺陷。
```
