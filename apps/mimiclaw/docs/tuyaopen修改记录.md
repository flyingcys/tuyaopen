## tuyaopen/src/tuya_cloud_service/transport/tcp_transporter.c lines 95-102
```c
  #if OPERATING_SYSTEM != SYSTEM_LINUX
      if (tcp_transporter->config.bindAddr == 0 && tcp_transporter->config.bindPort == 0) {
          NW_IP_S nw_ip = {0};
          if (OPRT_OK == netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_IP, &nw_ip) && nw_ip.ip[0] != '\0') {
              tcp_transporter->config.bindAddr = tal_net_str2addr(nw_ip.ip);
          }
      }
  #endif
```

结论：此 `#if` 是必要的，逻辑正确。
- 嵌入式平台（FreeRTOS + lwIP 等）：协议栈通常没有路由表，多网口（WiFi / 有线 / 蜂窝）并存时内核无法自动选择出口接口。必须主动 bind() 到 netmgr 报告的活跃接口 IP，否则 socket
可能走错接口或根本连不出去。
- Linux：OS 有完整的路由表（ip route），会自动根据目标地址选路。强行 bind() 到 netmgr 报告的 IP 反而可能破坏路由（VPN、容器网络、多网卡 policy routing 等场景都会出问题）。排除 Linux 是对的。