# System Architecture Specification — nordic-wifi-webdash v2.0

## Overview

Nordic WiFi Web Dashboard uses an **SMF + Zbus modular architecture**. Each feature lives in its own module under `src/modules/`. All inter-module communication is exclusively through Zbus channels. Modules initialize through `SYS_INIT` at priority-ordered boot time.

v2.0 adds a **Mode Selector** module and extends the **WiFi module** to support three runtime-selectable Wi-Fi roles: SoftAP, STA, and P2P (Wi-Fi Direct).

---

## Module Map

```
src/
├── main.c                        ← startup banner, SYS_INIT trigger
└── modules/
    ├── messages.h                ← all Zbus message structs (shared)
    ├── mode_selector/            ← NEW v2.0: boot-time mode selection
    ├── button/                   ← GPIO long-press detection (updated v2.0)
    ├── led/                      ← LED output control
    ├── wifi/                     ← multi-mode Wi-Fi (updated v2.0)
    ├── network/                  ← network event handlers
    ├── webserver/                ← HTTP server + REST API (updated v2.0)
    └── memory/                   ← heap monitor
```

---

## Zbus Channels

| Channel | Message Type | Publisher | Subscribers | Direction |
|---------|-------------|-----------|-------------|-----------|
| `WIFI_MODE_CHAN` | `struct wifi_mode_msg` | mode_selector | wifi | boot-time, once |
| `BUTTON_CHAN` | `struct button_msg` | button | webserver | runtime |
| `LED_CMD_CHAN` | `struct led_msg` | webserver | led | runtime |
| `LED_STATE_CHAN` | `struct led_state_msg` | led | webserver | runtime |
| `WIFI_CHAN` | `struct wifi_msg` | wifi | webserver, network | runtime |

### Message Definitions (`src/modules/messages.h`)

```c
/* Wi-Fi mode (new in v2.0) */
enum wifi_mode {
    WIFI_MODE_SOFTAP = 0,
    WIFI_MODE_STA    = 1,
    WIFI_MODE_P2P    = 2,
};

struct wifi_mode_msg {
    enum wifi_mode mode;
};

/* Button events */
enum button_msg_type {
    BUTTON_PRESSED,
    BUTTON_RELEASED,
};

struct button_msg {
    enum button_msg_type type;
    uint8_t  button_number;   /* 0-based DK index */
    uint32_t duration_ms;     /* press duration */
    uint32_t press_count;     /* total presses since boot */
    uint32_t timestamp;       /* k_uptime_get_32() */
    bool     is_boot_long_press; /* NEW v2.0: flagged during boot window */
};

/* LED commands */
enum led_msg_type {
    LED_COMMAND_ON,
    LED_COMMAND_OFF,
    LED_COMMAND_TOGGLE,
};

struct led_msg {
    enum led_msg_type type;
    uint8_t led_number;  /* 0-based DK index */
};

struct led_state_msg {
    uint8_t led_number;
    bool    is_on;
};

/* Wi-Fi connectivity status */
enum wifi_msg_type {
    WIFI_SOFTAP_STARTED,
    WIFI_STA_CONNECTED,
    WIFI_STA_DISCONNECTED,
    WIFI_P2P_CONNECTED,
    WIFI_P2P_DISCONNECTED,
    WIFI_ERROR,
};

struct wifi_msg {
    enum wifi_msg_type type;
    enum wifi_mode     active_mode;  /* NEW v2.0 */
    char               ip_addr[16];  /* NEW v2.0: dotted-decimal */
    char               ssid[33];     /* NEW v2.0 */
    int                error_code;
};
```

---

## SYS_INIT Priority Order

| Priority | Module | Function | Notes |
|----------|--------|----------|-------|
| 0 | mode_selector | `mode_selector_init` | Reads NVS mode; if Button 1 held, shows shell menu; publishes WIFI_MODE_CHAN |
| 1 | wifi | `wifi_module_init` | Reads WIFI_MODE_CHAN; starts SoftAP/STA/P2P path |
| 2 | button | `button_module_init` | GPIO IRQ setup |
| 3 | led | `led_module_init` | LED GPIO setup |
| 4 | network | `network_module_init` | Net mgmt event registration |
| 5 | webserver | `webserver_module_init` | HTTP server start (waits for WIFI_CHAN) |

Mode selector **must** run before wifi (priority 0 < 1) because WiFi module reads the published mode at init time.

---

## System Architecture Diagram

```mermaid
graph TB
    subgraph zbus_bus [Zbus Message Bus]
        WIFI_MODE_CHAN[WIFI_MODE_CHAN]
        BUTTON_CHAN[BUTTON_CHAN]
        LED_CMD_CHAN[LED_CMD_CHAN]
        LED_STATE_CHAN[LED_STATE_CHAN]
        WIFI_CHAN[WIFI_CHAN]
    end

    subgraph hw [Hardware Layer]
        HW_BTN["Button 1\n(+ Button 2 on nRF7002DK)\n(+ Button 0-3 on nRF54LM20DK)"]
        HW_LED["2 LEDs (nRF7002DK)\n4 LEDs (nRF54LM20DK)"]
        HW_WIFI[nRF7002 Wi-Fi IC]
        HW_UART[UART Console]
        HW_FLASH[Internal Flash\nNVS partition]
    end

    subgraph modules [Application Modules]
        ModeSel[Mode Selector\nboot-time NVS + shell menu]
        ButtonMod[Button Module\nSMF 3-state]
        LEDMod[LED Module\nSMF 2-state per LED]
        WiFiMod[WiFi Module\nSMF multi-path]
        NetMod[Network Module\nevent-driven]
        WebMod[Webserver Module\nHTTP handlers]
        MemMon[Memory Monitor\nheap stats]
    end

    HW_BTN -->|GPIO IRQ| ButtonMod
    HW_BTN -->|long-press at boot| ModeSel
    HW_FLASH <-->|NVS read/write| ModeSel
    HW_UART <-->|shell menu| ModeSel

    ModeSel -->|publish once| WIFI_MODE_CHAN
    WIFI_MODE_CHAN -->|subscribe| WiFiMod

    ButtonMod -->|publish| BUTTON_CHAN
    BUTTON_CHAN -->|subscribe| WebMod

    WebMod -->|publish| LED_CMD_CHAN
    LED_CMD_CHAN -->|subscribe| LEDMod
    LEDMod -->|publish| LED_STATE_CHAN
    LED_STATE_CHAN -->|subscribe| WebMod
    LEDMod -->|GPIO| HW_LED

    WiFiMod -->|publish| WIFI_CHAN
    WIFI_CHAN -->|subscribe| WebMod
    WIFI_CHAN -->|subscribe| NetMod
    WiFiMod <-->|Wi-Fi driver| HW_WIFI

    WebMod -->|HTTP server| Client[Web Browser]
```

---

## Boot Sequence

```mermaid
sequenceDiagram
    participant HW as Hardware Boot
    participant ModeSel as Mode Selector
    participant NVS as NVS Storage
    participant Shell as UART Shell
    participant WiFi as WiFi Module
    participant Web as Webserver
    participant Btn as Button Module

    HW->>ModeSel: SYS_INIT priority 0
    ModeSel->>NVS: Read "app/wifi_mode"
    alt First boot (no NVS entry)
        NVS-->>ModeSel: -ENOENT → use AP default
    else NVS entry exists
        NVS-->>ModeSel: stored mode (0/1/2)
    end

    ModeSel->>ModeSel: Check Button 1 GPIO state
    alt Button 1 held (>3s)
        ModeSel->>Shell: Print mode selection menu
        Shell->>ModeSel: User inputs 1 / 2 / 3
        ModeSel->>NVS: Save new mode
        ModeSel->>ModeSel: Update selected_mode
    end

    ModeSel->>WIFI_MODE_CHAN: Publish wifi_mode_msg

    HW->>WiFi: SYS_INIT priority 1
    WiFi->>WIFI_MODE_CHAN: Read mode
    alt AP mode
        WiFi->>WiFi: Start SoftAP path
    else STA mode
        WiFi->>WiFi: Start STA path (wifi_credentials)
    else P2P mode
        WiFi->>WiFi: Start P2P path (wifi p2p find)
    end

    HW->>Btn: SYS_INIT priority 2
    Btn->>Btn: Setup GPIO IRQs

    HW->>Web: SYS_INIT priority 5
    Web->>Web: Register HTTP routes
    Note over WiFi,Web: Web dashboard starts when WIFI_CHAN reports connected/started
```

---

## Wi-Fi Mode Paths

### SoftAP Path (mode = 0)

- Kconfig: `CONFIG_NRF70_AP_MODE=y`, `CONFIG_WIFI_NM_WPA_SUPPLICANT_AP=y`
- Static IP: `192.168.7.1/24`
- DHCP server: leases `192.168.7.2–192.168.7.3` (max 2 clients)
- HTTP server: `http://192.168.7.1` or `http://nrfwebdash.local`
- WiFi SSID: `CONFIG_APP_WIFI_SSID` (default `WebDash_AP`)

### STA Path (mode = 1)

- Kconfig: `CONFIG_WIFI_NM_WPA_SUPPLICANT=y`, `CONFIG_WIFI_CREDENTIALS=y`
- Credentials: stored via `wifi cred add "<SSID>" <sec_type> "<PSK>"` shell command
- IP: DHCP-assigned from AP
- HTTP server: `http://<dhcp-ip>` or `http://nrfwebdash.local`
- Connection Manager: `conn_mgr_all_if_connect()`

### P2P Path (mode = 2, nRF54LM20DK only)

- Kconfig: `CONFIG_NRF70_P2P_MODE=y`, `CONFIG_WIFI_NM_WPA_SUPPLICANT_P2P=y`
- Build: `-S wifi-p2p` snippet required
- Auto-start: `wifi p2p find` on boot
- Role: Group Interface (GI/client); phone acts as Group Owner (GO)
- IP: DHCP-assigned from phone's P2P group
- HTTP server: `http://<p2p-dhcp-ip>`
- Connection workflow (WCS-106):
  1. Device auto-starts `wifi p2p find`
  2. User runs `wifi p2p peer` to list discovered peers
  3. User runs `wifi p2p connect <MAC> pin -g 0` → pin shown on console
  4. User enters pin on phone
  5. DHCP IP received from phone → `WIFI_P2P_CONNECTED` published

---

## Board Capability Matrix

| Capability | nRF7002DK | nRF54LM20DK + nRF7002EBII |
|------------|-----------|---------------------------|
| Buttons at boot-select | 1 (Button 1) | 1 (BUTTON 0) |
| Total buttons | 2 | 3 (BUTTON 3 unavailable due to shield) |
| LEDs | 2 | 4 |
| SoftAP mode | Yes | Yes |
| STA mode | Yes | Yes |
| P2P mode | Shell only (no -S wifi-p2p in default build) | Yes (with -S wifi-p2p) |

---

## Memory Budget

### Baseline (v1.0 SoftAP only)

| Component | Flash | RAM |
|-----------|-------|-----|
| Wi-Fi Stack (SoftAP) | ~65 KB | ~50 KB |
| HTTP Server | ~25 KB | ~20 KB |
| SMF/Zbus | ~10 KB | ~5 KB |
| Application Modules | ~15 KB | ~10 KB |
| **Total** | **~115 KB** | **~85 KB** |

### v2.0 Additions

| New Feature | Flash | RAM | Notes |
|-------------|-------|-----|-------|
| Mode Selector + NVS | +8 KB | +3 KB | Settings + NVS + Flash storage |
| Wi-Fi STA path | +0 KB | +0 KB | Supplicant already linked; STA is subset |
| Wi-Fi Credentials | +5 KB | +2 KB | wifi_credentials library |
| P2P extensions | +5 KB | +3 KB | wpa_supplicant P2P (in -S wifi-p2p build only) |
| Webserver `/api/system` | +1 KB | +0 KB | Small handler addition |
| **v2.0 Total Delta** | **+19 KB** | **+8 KB** | P2P build adds ~5 KB flash |

Estimated v2.0 total (SoftAP/STA build): ~134 KB Flash, ~93 KB RAM
Estimated v2.0 total (P2P build): ~139 KB Flash, ~96 KB RAM

Available budget (nRF5340 app core): 1 MB Flash, 448 KB RAM — margins are comfortable.

---

## Related Specs

- [wifi-module.md](wifi-module.md) — SoftAP/STA/P2P paths, event flows, Kconfig
- [mode-selector.md](mode-selector.md) — boot window logic, NVS, shell menu
- [button-module.md](button-module.md) — long-press detection, boot flag
- [webserver-module.md](webserver-module.md) — mode-aware HTTP server, `/api/system`
