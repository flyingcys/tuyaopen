# Tuya Cloud Service Host Tests Phase 3 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 扩展 `src/tuya_cloud_service/test/host/`，完成 Phase 3 的 `tuya_weather`、`tuya_http`、`tuya_tls` Host 单测，并保持全量 Host 基线通过。

**Architecture:** 继续复用统一的 `src/tuya_cloud_service/test/host/` 入口，优先覆盖 service wrapper 的公开 contract：参数校验、网络/时间前置条件、请求封装和错误传播。对外部 HTTP/TLS/ATOP 依赖统一使用 test-local fake/stub，避免引入真实网络或证书资源。

**Tech Stack:** C, CMake, CTest, Unity, existing Host test support

---

## 执行状态（2026-03-11）

- [x] Task 1: 建立 `tuya_weather` Host 单元测试
- [x] Task 2: 建立 `tuya_http` Host 单元测试
- [x] Task 3: 建立 `tuya_tls` Host 单元测试
- [x] Task 4: 文档收口并同步 Phase 3 完成状态

### 当前进度备注

- 已建立 `test_tuya_cloud_service_weather_host`
- 已建立 `test_tuya_cloud_service_http_host`
- 已建立 `test_tuya_cloud_service_tls_host`
- 当前全量 Host 基线为 `13/13` suites 通过
