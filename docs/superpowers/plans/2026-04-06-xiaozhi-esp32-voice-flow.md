# xiaozhi ESP32 Voice Flow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 对齐 `/home/share/samba/xiaozhi-esp32` 的 Linux 语音链路，让 `apps/xiaozhi` 具备完整的 Snowboy 唤醒、提示音、本地状态切换、TTS 播放和自动恢复流程。

**Architecture:** 保持现有 `xiaozhi_app.c` 为运行时入口，在不重构传输层的前提下补齐 `listen detect`、`abort wake_word_detected`、`manual/auto` 模式下的 `tts stop` 转移，并把 Linux 音频门面从应用层解耦。提示音播放放在 `xiaozhi_audio_linux.c`，应用层只负责编排。

**Tech Stack:** C、C++、PortAudio、Opus、Snowboy、TuyaOpen `tos.py`

---

### Task 1: 补齐纯逻辑测试

**Files:**
- Modify: `apps/xiaozhi/tests/test_xiaozhi_state.c`
- Modify: `apps/xiaozhi/tests/test_xiaozhi_audio_linux.c`
- Create: `apps/xiaozhi/tests/test_xiaozhi_protocol.c`

- [ ] 增加 `tts stop` 在 `auto/manual` 模式下的状态转移测试。
- [ ] 增加 `audio_linux` test-only 编译测试，覆盖空包拒绝和热词/提示音门面可调用。
- [ ] 增加 `listen detect` 与 `abort reason=wake_word_detected` 的 JSON 构造测试。

### Task 2: 解耦 Linux 音频门面

**Files:**
- Modify: `apps/xiaozhi/src/xiaozhi_audio_linux.h`
- Modify: `apps/xiaozhi/src/xiaozhi_audio_linux.c`

- [ ] 去掉 `xiaozhi_audio_linux.c` 对 `xiaozhi_app.h` 的直接依赖，改为热词回调注册。
- [ ] 新增本地提示音播放接口，默认播放 `apps/xiaozhi/src/resources/waked.pcm`。
- [ ] 保持现有 PortAudio/Snowboy 线程结构不变，只补齐提示音与回调编排。

### Task 3: 对齐 app 状态与传输语义

**Files:**
- Modify: `apps/xiaozhi/src/xiaozhi_app.c`
- Modify: `apps/xiaozhi/src/xiaozhi_state.h`
- Modify: `apps/xiaozhi/src/xiaozhi_state.c`

- [ ] 给传输回调加锁，避免 CLI 和 worker 并发改状态。
- [ ] 唤醒时改为 `listen detect -> 本地提示音 -> listen start auto`。
- [ ] 在 `speaking/listening` 中再次唤醒时先发 `abort(wake_word_detected)`，再重新进入自动监听。
- [ ] `tts stop` 时按 `manual/auto/realtime` 决定回 `idle` 还是继续 `listening`。
- [ ] 连接成功后默认启用检测，对齐 `xiaozhi-esp32` 空闲态自动唤醒行为。

### Task 4: 资源与验证

**Files:**
- Create: `apps/xiaozhi/src/resources/waked.pcm`
- Modify: `apps/xiaozhi/README.md`
- Modify: `apps/xiaozhi/README_CN.md`

- [ ] 引入提示音资源并在文档中说明默认路径和可选环境变量。
- [ ] 运行最小单测、`tos.py build` 和相关格式检查。
