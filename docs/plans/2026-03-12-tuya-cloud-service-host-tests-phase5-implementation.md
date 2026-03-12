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
- 当前全量 Host 基线为 `21/21` suites 通过
