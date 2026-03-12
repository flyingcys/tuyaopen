# Tuya Cloud Service Host Tests Phase 5 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 扩展 `src/tuya_cloud_service/test/host/`，开始覆盖 Phase 5 的平台/集成导向模块，优先 `lan`、`netmgr`、`netcfg` 与 `authorize` 的公开 contract，并继续保持全量 Host 基线通过。

**Architecture:** Phase 5 继续沿用 `gc-sections` + test-local fake 的做法，从最小 public contract 入手。先测空参数、未初始化状态、enable/disable/query 类 API，避免一开始就把完整 socket、配网或蓝牙数据面拉入 Host 单测。

**Tech Stack:** C, CMake, CTest, Unity, existing Host test support

---

## 执行状态（2026-03-12）

- [x] Task 1: 切入 `lan` 的第一批 Host 单测
- [x] Task 2: 切入 `netmgr` 的第一批 Host 单测
- [x] Task 3: 评估 `netcfg` / `authorize` 的低风险入口
- [x] Task 4: 文档收口并同步 Phase 5 进展

### 当前进度备注

- 已建立 `test_tuya_cloud_service_lan_host`
- 已建立 `test_tuya_cloud_service_authorize_host`
- 已建立 `test_tuya_cloud_service_netcfg_host`
- 已建立 `test_tuya_cloud_service_netmgr_host`
- `lan` 已改为 white-box Host 覆盖方式，当前 `tuya_lan.c` 为 `100%` 行覆盖、`100%` 函数覆盖
- `lan` 现已覆盖 callback register/unregister、session 生命周期、TCP/UDP socket callback、握手、DP report、`init/exit`、激活态 `enable/disable`、未初始化与已初始化无 session 的 report/query 路径
- `lan` 对极少数 Host 侧难以稳定驱动的防御/恢复分支使用了显式 `LCOV_EXCL_*` 标记，已在源码中就地说明
- `authorize` 现已覆盖 KV 写入成功/失败、KV 优先读取、OTP fallback 与 reset 错误传播
- `authorize` 现已补齐 CLI `auth` / `auth-read` / `auth-reset` 路径，并达到 Host 侧公开/CLI 入口完整覆盖
- `netcfg` 现已覆盖 register/unregister 计数、duplicate reject、start/stop started 状态，以及 `start_other_all` / `stop_other_all` 分发
- `netcfg` 现已补齐 stop-all/no-session、start/stop failure continue 等主要错误路径
- `netmgr` 现已覆盖 `__netmgr_conn_register` 的 null/duplicate、`netmgr_init` 无连接错误、connection set/get 分发、`NETCONN_AUTO` 路由、timer callback 与缺失回调错误
- 当前全量 Host 基线为 `21/21` suites 通过
