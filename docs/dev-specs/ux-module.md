# UX Module Specification — nordic-wifi-webdash

## Document Information

| Field | Value |
|---|---|
| Project | nordic-wifi-webdash |
| Version | 2026-06-19-13-12 |
| PRD Version | 2026-06-19-13-12 |
| NCS Version | v3.3.0 |
| Target Board(s) | nRF54LM20DK + nRF7002EB2, nRF7002DK |
| Status | Specified |

## Changelog

| Version | Summary |
|---|---|
| 2026-06-19-13-12 | PRD Version updated to 2026-06-19-13-12. |
| 2026-06-17-14-22 | Initial spec — ported from `zego/nordic-wifi-app-template/src/modules/ux/ux.c`; covers FR-106 (Button 0 gestures + LED 0 Wi-Fi state machine) |

---

## Overview

The `ux` module provides the out-of-box hardware UX for **Button 0** and **LED 0** on both supported boards.

It is ported verbatim from `zego/nordic-wifi-app-template/src/modules/ux/` with no modifications
needed — the module reads `APP_WIFI_STATE_CHAN` (defined in `messages.h` and published by
`net_event_app.c`) and `BUTTON_CHAN` (published by the external `zego/button` module).

---

## Location

- **Path**: `src/modules/ux/`
- **Files**: `ux.c`, `Kconfig`, `CMakeLists.txt`
- **Kconfig symbol**: `APP_UX_MODULE` (depends on `ZEGO_BUTTON && ZEGO_LED && ZEGO_WIFI && REBOOT && SETTINGS`)

---

## Zbus Integration

**Subscribes to**:
- `BUTTON_CHAN` — Button 0 gesture events from `zego/button`
- `APP_WIFI_STATE_CHAN` — connectivity state changes from `net_event_app.c`

**Publishes to**:
- `LED_CMD_CHAN` — LED 0 effect commands (ROTATE, ON, BLINK, BREATHE)

---

## Button 0 Gesture Map (FR-106)

| Gesture | Action |
|---------|--------|
| `BUTTON_SINGLE_CLICK` | `LOG_INF` current Wi-Fi mode to UART log |
| `BUTTON_DOUBLE_CLICK` | Toggle LED 0 between BREATHE (BLE prov indicator) and last Wi-Fi state |
| `BUTTON_LONG_PRESS` | Cycle mode STA → SoftAP → P2P_GO → P2P_CLIENT → STA; save to NVS via `settings_save_one("app/app_wifi_mode", ...)`; `sys_reboot(SYS_REBOOT_COLD)` |

Long-press acknowledgement: LED 0 turns OFF for 300 ms before reboot so the user sees a
visual blink confirming the press was registered.

---

## LED 0 Wi-Fi State Machine

Driven by messages on `APP_WIFI_STATE_CHAN`:

| State (`enum app_wifi_state`) | LED 0 effect |
|-------------------------------|-------------|
| `APP_WIFI_STATE_CONNECTING` | `LED_COMMAND_ROTATE` (starts at `SYS_INIT`) |
| `APP_WIFI_STATE_CONNECTED` | `LED_COMMAND_ON` (solid) |
| `APP_WIFI_STATE_SOFTAP` | `LED_COMMAND_ROTATE` (AP up, same as connecting) |
| `APP_WIFI_STATE_ERROR` | `LED_COMMAND_BLINK`, period_ms = 100 ms (fast blink) |

BLE provisioning mode (double-click toggle) interrupts the state machine locally:
- Double-click → `LED_COMMAND_BREATHE` on LED 0
- Next double-click → restores last Wi-Fi state LED effect

---

## `APP_WIFI_STATE_CHAN` Message Type

Defined in `src/modules/messages.h`:

```c
enum app_wifi_state {
    APP_WIFI_STATE_CONNECTING = 0,
    APP_WIFI_STATE_CONNECTED,
    APP_WIFI_STATE_SOFTAP,
    APP_WIFI_STATE_ERROR,
};

struct app_wifi_state_msg {
    enum app_wifi_state state;
    enum zego_wifi_mode mode;
};

ZBUS_CHAN_DECLARE(APP_WIFI_STATE_CHAN);
```

`APP_WIFI_STATE_CHAN` is **defined** (via `ZBUS_CHAN_DEFINE`) in `net_event_app.c`.

---

## Kconfig Options

| Symbol | Default | Description |
|--------|---------|-------------|
| `APP_UX_MODULE` | `n` | Enable the UX module |
| `APP_UX_INIT_PRIORITY` | `95` | SYS_INIT priority; must be > `ZEGO_LED_INIT_PRIORITY` (91) |
| `APP_UX_ROTATE_FIRST_LED` | `0` | First LED in ROTATE sweep |
| `APP_UX_ROTATE_COUNT` | `0` | Number of LEDs in ROTATE sweep (0 = all) |
| `APP_UX_CONNECTED_LED` | `0` | LED index for solid-ON when connected |

---

## SYS_INIT

```c
SYS_INIT(app_ux_init, APPLICATION, CONFIG_APP_UX_INIT_PRIORITY);
```

`app_ux_init` registers Zbus observers for `BUTTON_CHAN` and `APP_WIFI_STATE_CHAN`,
then publishes an initial `LED_COMMAND_ROTATE` on `LED_CMD_CHAN` to show the
"connecting" indication at boot.

---

## Test Cases

### TC-UX-001: Single-click mode print

1. Boot device; connect serial terminal.
2. Single-click Button 0.
3. Verify UART log contains `INF: Mode: <current mode>`.

### TC-UX-002: Long-press mode cycle

1. Boot in STA mode.
2. Long-press Button 0 (> `CONFIG_ZEGO_BUTTON_LONG_PRESS_MS`, default 1000 ms).
3. Verify LED 0 blinks off briefly, then device reboots in SoftAP mode.
4. Repeat to cycle through P2P_GO → P2P_CLIENT → STA.

### TC-UX-003: LED state follows Wi-Fi state

1. Boot device; verify LED 0 ROTATEs during Wi-Fi connection attempt.
2. In STA mode: after `wifi connect` succeeds, verify LED 0 goes solid ON.
3. Disconnect from AP; verify LED 0 returns to ROTATE (or BLINK if error).

---

## Related Specs

- [architecture.md](architecture.md) — Zbus channel table, SYS_INIT priorities
- [network-module.md](network-module.md) — `APP_WIFI_STATE_CHAN` publisher
- Source reference: `zego/nordic-wifi-app-template/src/modules/ux/ux.c`
