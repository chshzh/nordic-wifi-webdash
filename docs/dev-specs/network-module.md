# Network Module Specification

## Document Information

| Field | Value |
|---|---|
| Project | nordic-wifi-webdash |
| Version | 2026-06-04-23-45 |
| PRD Version | 2026-06-04-23-14 |
| NCS Version | v3.3.0 |
| Target Board(s) | nRF7002DK, nRF54LM20DK + nRF7002EB2 |
| Status | Implemented |

> **Note**: Wi-Fi event handling is provided by `zego/modules/network` (registered via `EXTRA_ZEPHYR_MODULES`).
> The canonical spec is at **[zego/network ↗](https://github.com/chshzh/zego/blob/main/modules/network/docs/network-spec.md)**.
> This file documents the app-specific integration layer (`net_event_app.c`).

## Changelog

| Version | Summary |
|---|---|
| 2026-06-04-23-14 | Updated Location section: old `net_event_mgmt.c/h`, `wifi_utils.c/h` removed; replaced by `net_event_app.c` (zego/network weak-hook overrides). Added zego/network canonical spec reference. |
| 2026-06-04-23-45 | Trimmed to app-layer shim only (`net_event_app.c`): replaced full SoftAP/STA/P2P content with Channel Definitions and Weak Hook Overrides; all Wi-Fi mode/protocol details belong in zego/network canonical spec. |
| 2026-06-04-23-30 | Added proper Document Information table. |
| 2026-04-14-11-00 | P2P_GO auto-start: group_add + wps_pin called automatically at boot; 5-min wait timer for first client; state machine updated with P2P_GO_Waiting state; WPS PIN logged to console |
| 2026-04-14-10-00 | Code sync: P2P split into P2P_GO (device is GO) and P2P_CLIENT (device joins phone group); WIFI_CHAN+wifi_msg → CLIENT_CONNECTED_CHAN+dk_wifi_info_msg; WPS PIN Kconfig added; SoftAP/P2P_GO publish on AP_STA_CONNECTED; STA/P2P_CLIENT publish on DHCP_BOUND; DK MAC filled from net_if_get_link_addr |
| 2026-04-09-14-00 | Renamed from wifi-module.md to network-module.md to match src/modules/network/ directory |
| 2026-04-09-12-00 | STA: session-based connection (`wifi connect`) replaces stored credentials / conn_mgr auto-connect; P2P: now supported on both boards with `-DSNIPPET=wifi-p2p`; P2P connect method updated to `pbc` |
| 2026-03-31 | v2.0 — multi-mode SoftAP/STA/P2P controller |

---

## Overview

`net_event_app.c` bridges `zego/modules/network` events to the `nordic-wifi-webdash` application. It defines `CLIENT_CONNECTED_CHAN` and overrides the `__weak` hooks from `zego/network` to publish connectivity events in the app-specific message format.

Wi-Fi mode selection, the connection state machine, SoftAP/STA/P2P paths, Kconfig options, and memory footprint are documented in the canonical spec: **[zego/network ↗](https://github.com/chshzh/zego/blob/main/modules/network/docs/network-spec.md)**

---

## Location

- **Path**: `src/modules/network/`
- **Files**: `net_event_app.c`, `CMakeLists.txt`
- The underlying Wi-Fi event management is handled by `zego/modules/network` via `EXTRA_ZEPHYR_MODULES`.

---

## Channel Definitions

| Channel | Type | Defined in |
|---------|------|------------|
| `CLIENT_CONNECTED_CHAN` | `struct dk_wifi_info_msg` | `net_event_app.c` |

```c
struct dk_wifi_info_msg {
    enum app_wifi_mode active_mode; /* Wi-Fi mode (SoftAP / STA / P2P_GO / P2P_CLIENT) */
    char dk_ip_addr[16];            /* Device IP (dotted-decimal) */
    char dk_mac_addr[18];           /* Device MAC (XX:XX:XX:XX:XX:XX) */
    char ssid[33];                  /* Connected / hosted SSID */
    int  error_code;
};
```

---

## Weak Hook Overrides

| Hook | Publishes | Condition |
|------|-----------|----------|
| `zego_network_on_wifi_connected(mode, ip, mac, ssid)` | `CLIENT_CONNECTED_CHAN` | Always |
| `zego_network_on_wifi_connected(...)` | `WIFI_CHAN` (`WIFI_STA_CONNECTED`) | `CONFIG_ZEGO_WIFI_BLE_PROV=y` only |
| `zego_network_on_wifi_disconnected()` | `WIFI_CHAN` (`WIFI_STA_DISCONNECTED`) | `CONFIG_ZEGO_WIFI_BLE_PROV=y` only |

No `CLIENT_CONNECTED_CHAN` publish on disconnect.

---

## Related Specs

- [architecture.md](architecture.md) — zbus channel map, SYS_INIT priorities
- [zego/network ↗](https://github.com/chshzh/zego/blob/main/modules/network/docs/network-spec.md) — Wi-Fi modes, state machine, SoftAP/STA/P2P, Kconfig
- [webserver-module.md](webserver-module.md) — dashboard IP display per mode
