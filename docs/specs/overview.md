# Engineering Specs Overview — Nordic Wi-Fi WebDash

## Document Information

| Field | Value |
|-------|-------|
| Project | nordic-wifi-webdash |
| Version | 2026-04-17-10-00 |
| PRD Version | 2026-04-17-10-00 |
| Author | Charlie Shao |
| NCS Version | v3.3.0 |
| Target Board(s) | nRF7002DK, nRF54LM20DK + nRF7002EBII |
| Status | Current |

---

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-04-17-10-00 | Add FR-105 dark mode to webserver-module.md: CSS tokens, `prefers-color-scheme` auto-detect, manual toggle, TC-WEB-006; PRD Version bumped |
| 2026-04-14-15-00 | Startup: APP_VERSION_STRING injected at build time (git tag / v<NCS>-dev); banner aligned labels + module boot sequence; SoftAP periodic reminder timer (300 s, sysworkq, cancelled on first client); PRD Version bumped |
| 2026-04-14-11-00 | P2P_GO auto-start feature: group_add + wps_pin run automatically at boot; 5-min client wait timer; PRD Version bumped |
| 2026-04-14-10-00 | Code sync: P2P split into P2P_GO/P2P_CLIENT; default mode changed to P2P_GO; WIFI_CHAN+wifi_msg → CLIENT_CONNECTED_CHAN+dk_wifi_info_msg; WPS Kconfig added; /api/system fields updated; client IP tracked in all modes |
| 2026-04-09-14-00 | Code alignment review: all spec gaps resolved (module paths, SYS_INIT priorities, button_msg struct, DNS-SD macro); rename wifi-module.md → network-module.md; add led-module.md |
| 2026-04-09-14-00 | Initial overview — written against PRD 2026-04-09-12-00; mode selector is shell-command driven; P2P both boards; STA session-based |

---

## 1. Purpose

This document is the entry point for the engineering specs of `nordic-wifi-webdash`.
It maps product requirements to spec files and captures top-level design decisions.

For the product requirements that drive this design, see [`docs/PRD.md`](../PRD.md).

---

## 2. Spec Index

| Spec file | Covers | PRD sections |
|-----------|--------|--------------|
| [architecture.md](architecture.md) | System overview, module map, Zbus channels, SYS_INIT boot sequence, memory budget | All |
| [network-module.md](network-module.md) | SoftAP / STA / P2P paths, net event handling, WPA supplicant integration | FR-001, FR-002, FR-003, FR-101, FR-102, FR-201 |
| [mode-selector.md](mode-selector.md) | `app_wifi_mode` shell command, NVS persistence, factory default | FR-004, FR-005 |
| [webserver-module.md](webserver-module.md) | HTTP server, REST API endpoints, DNS-SD `_http._tcp.local`, mode banner, web UI | FR-006, FR-007, FR-101, FR-104 |
| [button-module.md](button-module.md) | GPIO button monitoring, SMF press/release events, board differences | FR-006 |
| [led-module.md](led-module.md) | LED control, SMF 2-state per LED, REST-commanded via LED_CMD_CHAN | FR-006 |

---

## 3. Architecture Summary

**Pattern**: SMF + Zbus modular

All feature modules live under `src/modules/<name>/`. No module calls another module's functions directly — all inter-module communication is through Zbus channels only. Modules start deterministically via `SYS_INIT` priority ordering.

**Key design decisions:**

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Architecture pattern | SMF + Zbus | Decoupled modules; clear publish/subscribe ownership; Zbus is the only inter-module channel |
| Mode selection UX | `app_wifi_mode` shell command (runtime, any time) | Removes boot-time button dependency; works without physical access to the board |
| STA credentials | Session-based (`wifi connect` shell command) | Credentials never stored in NVS or source control; no data-at-rest credential exposure |
| P2P availability | Both boards with `-DSNIPPET=wifi-p2p` | nRF7002 P2P works on nRF7002DK and nRF54LM20DK + nRF7002EBII via the same snippet |
| P2P roles | P2P_GO (device is Group Owner) and P2P_CLIENT (device joins phone group) | Two distinct modes allow full flexibility: host your own group or join the phone's group |
| P2P pairing method | PBC (`wifi p2p connect <MAC> pbc`) or WPS PIN (`wifi wps_pin`) | WPS PIN adds a second connection method; PBC remains the simpler demo path |
| Default mode on fresh flash | P2P_GO | Enables out-of-box P2P demo without requiring SoftAP or STA credential setup |
| Service discovery | mDNS hostname + DNS-SD `_http._tcp.local` | Device reachable by name and discoverable without the user knowing its IP |
| Credential files | `overlay-wifi-credentials.conf` gitignored | Keeps secrets out of source control per NFR-SEC-001 |

---

## 4. PRD-to-Spec Mapping

| PRD requirement | Spec file | Status |
|----------------|-----------|--------|
| FR-001 SoftAP mode | [network-module.md](network-module.md) | Specified |
| FR-002 STA mode (session-based) | [network-module.md](network-module.md) | Specified |
| FR-003 P2P / Wi-Fi Direct — P2P_GO role | [network-module.md](network-module.md) | Specified |
| FR-003 P2P / Wi-Fi Direct — P2P_CLIENT role | [network-module.md](network-module.md) | Specified |
| FR-004 `app_wifi_mode` shell command | [mode-selector.md](mode-selector.md) | Specified |
| FR-005 Mode persisted in NVS | [mode-selector.md](mode-selector.md) | Specified |
| FR-006 Buttons & LEDs in browser | [webserver-module.md](webserver-module.md), [button-module.md](button-module.md), [led-module.md](led-module.md) | Specified |
| FR-007 Mode banner + IP in dashboard | [webserver-module.md](webserver-module.md) | Specified |
| FR-101 REST API | [webserver-module.md](webserver-module.md) | Specified |
| FR-102 Shell commands for diagnostics | [network-module.md](network-module.md), [mode-selector.md](mode-selector.md) | Specified |
| FR-103 Startup log | [architecture.md](architecture.md) | Specified |
| FR-104 DNS-SD `_http._tcp.local` | [webserver-module.md](webserver-module.md) | Specified |
| FR-201 Customisable SoftAP credentials | [network-module.md](network-module.md) | Specified |
| FR-202 Heap usage logging | [architecture.md](architecture.md) | Specified |
| NFR — Performance targets | [architecture.md](architecture.md) | Specified |
| NFR — 24-hour soak | [architecture.md](architecture.md) | Specified |
| NFR — Credentials security | [network-module.md](network-module.md), [mode-selector.md](mode-selector.md) | Specified |

---

## 5. Module Dependency Map

```
                       ┌─────────────────────────┐
                       │    NVS / Flash           │
                       └──────────┬──────────────┘
                                  │ read/write
                       ┌──────────▼──────────────┐
                       │    mode_selector         │◄── uart: app_wifi_mode command
                       └──────────┬──────────────┘
                                  │ WIFI_MODE_CHAN
               ┌──────────────────▼──────────────────────┐
               │              wifi / network              │
               │  SoftAP path │ STA path │ P2P path       │
               └──┬──────────────────────────────────┬───┘
                  │ WIFI_CHAN                         │ WIFI_CHAN
     ┌────────────▼──────────┐          ┌────────────▼──────────┐
     │      webserver        │          │  (other subscribers)  │
     │  HTTP + REST + DNS-SD │          └────────────────────────┘
     └────────────┬──────────┘
                  │ LED_CMD_CHAN
     ┌────────────▼──────────┐
     │         led           │
     └───────────────────────┘

     button ──BUTTON_CHAN──► webserver
     led    ──LED_STATE_CHAN─► webserver
```

> For the full Zbus channel table and message struct definitions, see [architecture.md](architecture.md).

---

## 6. Open Issues

| # | Description | Owner | Target |
|---|-------------|-------|--------|
| 1 | Should the web UI display RSSI in STA mode? (PRD Q1) | PM | TBD |
| 2 | Should Button 2 have a dedicated function (e.g. trigger STA reconnect)? (PRD Q2) | PM | TBD |

*(Changelog is maintained at the top of this document.)*
