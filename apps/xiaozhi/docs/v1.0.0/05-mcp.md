# v1.0.0 MCP 协议与工具调用

## 1. 适用范围

代码位置：`apps/xiaozhi/src/xiaozhi_mcp.c`

MCP 使用 `type = "mcp"` 的外层消息，内部 `payload` 为 JSON-RPC 2.0。

## 2. 外层封装格式

设备和服务端之间通过如下结构承载 MCP：

```json
{
  "session_id": "xxx",
  "type": "mcp",
  "payload": {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "tools/list",
    "params": {}
  }
}
```

## 3. 当前支持的典型 RPC

### 3.1 initialize

用于初始化 MCP 能力协商，通常会交换：

- `protocolVersion`
- `capabilities`
- `serverInfo`

### 3.2 tools/list

用于列出当前设备暴露的工具列表。

### 3.3 tools/call

用于实际调用设备工具。

## 4. 当前内置工具

从 `xz_mcp_tool_t` 可见，当前内置工具包括：

- `self.get_device_status`
- `self.get_system_info`
- `self.reboot`
- `self.upgrade_firmware`
- `self.assets.set_download_url`

## 5. 工具说明

### 5.1 self.get_device_status

用于查询当前设备状态，例如：

- 音频
- 屏幕
- 电池
- 网络

### 5.2 self.get_system_info

用于获取系统信息。

### 5.3 self.reboot

用于触发设备重启。

### 5.4 self.upgrade_firmware

用于指定固件 URL 执行升级。

其 `inputSchema` 中的 `url` 字段为字符串类型，并带默认说明。

### 5.5 self.assets.set_download_url

用于设置资源下载地址。  
该工具依赖 `assets partition_valid` 条件，若资源分区不可用，该工具不会对外暴露。

## 6. 返回结果格式

工具调用结果通常包装成：

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "..."
      }
    ],
    "isError": false
  }
}
```

## 7. 错误处理行为

当前实现特别对齐了一个上游行为：

- 当 `params` 不是 object 时，静默丢弃，不返回 RPC error

这点在兼容性审计文档中已经明确记录。

## 8. 能力协商中的扩展字段

当前实现还会解析某些能力信息，例如：

- `vision.url`
- `vision.token`

并将其落入本地配置。

## 9. MCP 在小智中的作用

MCP 是设备能力发现和工具调用的统一通道，适合承载：

- 设备状态查询
- 系统命令
- 固件升级触发
- 后续的 IoT 工具扩展

在语音会话中，服务端可通过 LLM 决策是否调用设备工具，再用 MCP 下发调用请求。
