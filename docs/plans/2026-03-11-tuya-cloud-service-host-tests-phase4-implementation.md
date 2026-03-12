# Tuya Cloud Service Host Tests Phase 4 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 扩展 `src/tuya_cloud_service/test/host/`，开始覆盖 Phase 4 的主链路 orchestrator，优先 `tuya_iot.c`，随后再进入 `mqtt_service.c`、`atop_service.c`、`matop_service.c`。

**Architecture:** Phase 4 不再以纯 wrapper 为主，而是围绕 `tuya_iot` 的公开状态 API、JSON report helper、token register 和 version update 参数校验做第一批 Host 测试。先挑“可通过 fake client + fake storage + fake publish path” 驱动的公开 contract，避免一开始就拉通完整状态机。

**Tech Stack:** C, CMake, CTest, Unity, existing Host test support

---

## 执行状态（2026-03-11）

- [x] Task 1: 建立 `tuya_iot` 第一批 Host 单元测试
- [x] Task 2: 扩展 `tuya_iot` report/token/version 路径
- [x] Task 3: 评估并切入 `mqtt_service` / `atop_service`
- [x] Task 4: 文档收口并同步 Phase 4 进展

### 当前进度备注

- 已建立 `test_tuya_cloud_service_iot_host`
- 已建立 `test_tuya_cloud_service_mqtt_host`
- 已建立 `test_tuya_cloud_service_atop_host`
- 已建立 `test_tuya_cloud_service_matop_host`
- 当前全量 Host 基线为 `17/17` suites 通过

### 首批推荐切入点

- `tuya_iot_activated`
- `tuya_iot_token_get_port_register`
- `tuya_iot_extension_modules_version_update`
- `tuya_iot_dp_report_json_async/with_notify/with_time/json`
- `tuya_iot_devid_get/localkey_get/seckey_get/timezone_get`

### 暂缓项

- 完整 `tuya_iot_init/start/stop/yield` 状态机
- `mqtt_service` 的完整 publish/yield path
- `atop_service` / `matop_service` 的真实 request flow

这些内容仍会进入 Phase 4，但要等 `tuya_iot` 的 fake client / fake publish / fake kv 模式稳定后再继续。
