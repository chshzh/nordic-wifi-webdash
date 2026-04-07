# Nordic WiFi Web Dashboard

[![Build](https://github.com/chshzh/nordic-wifi-webdash/actions/workflows/build.yml/badge.svg)](https://github.com/chshzh/nordic-wifi-webdash/actions/workflows/build.yml)
[![NCS Version](https://img.shields.io/badge/NCS-v3.2.4-green.svg)](https://www.nordicsemi.com/Products/Development-software/nRF-Connect-SDK)
![Nordic Semiconductor](https://img.shields.io/badge/Nordic%20Semiconductor-nRF7002DK-blue)
![Nordic Semiconductor](https://img.shields.io/badge/Nordic%20Semiconductor-nRF54LM20DK%2BnRF7002EBII-red)
[![License](https://img.shields.io/badge/License-LicenseRef--Nordic--5--Clause-blue.svg)](LICENSE)

A professional IoT demo and reference platform for Nordic **nRF7x series Wi-Fi development kits**. Provides real-time device state control (buttons, LEDs) through a browser-based dashboard served directly from the nRF device — no cloud required.

**v2.0** upgrades from single SoftAP mode to a **three-mode Wi-Fi platform**: SoftAP, STA (Station), and P2P (Wi-Fi Direct). The active mode is persisted in NVS and can be changed at any time via the `wifi_mode` shell command — the board reboots automatically to apply the new mode.

## 🎯 Features

- ✅ **Three Wi-Fi Modes** — SoftAP (own SoftAP), STA (join existing network), P2P (Wi-Fi Direct to phone)
- ✅ **Runtime Mode Selector** — `wifi_mode [SoftAP|STA|P2P]` changes mode at any time; saved to NVS, board reboots
- ✅ **Mode-Aware Dashboard** — colour-coded mode banner (blue=SoftAP, green=STA, purple=P2P)
- ✅ **REST API** — `/api/system`, `/api/buttons`, `/api/leds`, `/api/led`
- ✅ **Static Web Server** — gzip-compressed HTML/CSS/JS served from flash
- ✅ **Button Monitoring** — real-time state and press count (2 buttons on nRF7002DK, 3 on nRF54LM20DK)
- ✅ **LED Control** — ON/OFF/Toggle per LED (2 LEDs on nRF7002DK, 4 on nRF54LM20DK)
- ✅ **SMF + Zbus Architecture** — modular, event-driven, production-ready
- ✅ **Auto-Refresh** — 500 ms polling for button/LED/system state
- ✅ **mDNS** — accessible at `http://nrfwifi.local` in SoftAP and STA modes

## 📁 Project Structure

```
nordic-wifi-webdash/
├── CMakeLists.txt           # Main build configuration
├── Kconfig                  # Kconfig menu
├── prj.conf                 # Project configuration (SoftAP + STA)
├── LICENSE                  # Nordic 5-Clause license
├── README.md                # This file
├── .gitignore
│
├── boards/                  # Board-specific configs
│
├── src/
│   ├── main.c              # Startup banner
│   └── modules/
│       ├── messages.h           # Shared Zbus message types
│       ├── mode_selector/       # NEW v2.0 — runtime mode selection + NVS
│       ├── button/              # GPIO button, press events
│       ├── led/                 # LED state control
│       ├── wifi/                # Multi-mode WiFi (SoftAP/STA/P2P)
│       ├── network/             # Net mgmt event handler
│       ├── webserver/           # HTTP server + REST API
│       └── memory/              # Heap monitor
│
└── www/                    # Web dashboard files
    ├── index.html           # Mode banner + button/LED panels
    ├── main.js              # /api/system polling, mode colours
    └── styles.css
```
## 🚀 Quick Start

### Step 1 — Flash the firmware

Download the pre-built `.hex` for your board from the [Releases](https://github.com/chshzh/nordic-wifi-webdash/releases) page, then open **nRF Connect for Desktop → Programmer**, select your board, add the `.hex` file, and click **Write**.

| Board | Download |
|-------|----------|
| nRF7002DK | [nordic-wifi-webdash-nrf7002dk-latest.hex](https://github.com/chshzh/nordic-wifi-webdash/releases/latest) |
| nRF54LM20DK + nRF7002EBII | [nordic-wifi-webdash-nrf54lm20dk-nrf7002ebii-latest.hex](https://github.com/chshzh/nordic-wifi-webdash/releases/latest) |

### Step 2 — Choose Wi-Fi mode and open the dashboard

Open a serial terminal at **115200 baud** and read the boot log — it prints the exact next steps for the active mode.

| Mode | How to activate | Dashboard URL |
|------|----------------|---------------|
| **SoftAP** (default) | No action needed — boots directly into AP | Connect to `WebDashboard_AP` / `12345678`, then open `http://192.168.7.1` |
| **STA** | `uart:~$ wifi_mode STA` → reboot → `wifi connect -s <SSID> -p <pass> -k 1` | `http://<DHCP-IP>` shown in the boot log |
| **P2P** | `uart:~$ wifi_mode P2P` → reboot, follow Wi-Fi Direct instructions in terminal | `http://192.168.49.x` |

> **Switching modes:** run `uart:~$ wifi_mode [SoftAP|STA|P2P]` at any time — the board saves the choice to NVS and reboots automatically.

### Build from source

- nRF Connect SDK **v3.2.4**
- Supported hardware (both support SoftAP + STA + P2P when built with `-S wifi-p2p`):
  - nRF7002DK (`nrf7002dk/nrf5340/cpuapp`)
  - nRF54LM20DK + nRF7002EBII (`nrf54lm20dk/nrf54lm20a/cpuapp` + shield)

### Build & Flash

```bash
# nRF7002DK — SoftAP + STA + P2P
west build -p -b nrf7002dk/nrf5340/cpuapp -S wifi-p2p
west flash

# nRF54LM20DK + nRF7002EBII shield — SoftAP + STA + P2P
west build -p -b nrf54lm20dk/nrf54lm20a/cpuapp -S wifi-p2p -- -DSHIELD=nrf7002eb2
west flash
```

> **Note (nRF54LM20DK + nRF7002EBII):** BUTTON3 is unavailable due to shield pin conflicts. BUTTON0–BUTTON2 and all 4 LEDs are functional.

### Mode Selection

| Mode | How to connect | Dashboard URL |
|------|---------------|---------------|
| **SoftAP** (default) | Connect to Wi-Fi `WebDashboard_AP` (password `12345678`) | `http://192.168.7.1` |
| **STA** | Type `wifi connect <SSID> <password>` in serial shell | `http://<DHCP-IP>` |
| **P2P** | Wi-Fi Direct from phone | `http://192.168.49.x` |

**To change mode**, open a serial terminal (`uart:~$`) and run the `wifi_mode` command at any time:

```
uart:~$ wifi_mode STA
Switching to STA mode -- rebooting...
[00:00:xx.xxx] <inf> mode_selector: Mode saved: STA -> rebooting
*** Booting nRF Connect SDK v3.2.4 ***
[00:00:00.163,283] <inf> mode_selector: Stored mode: STA
```

The board saves the new mode to NVS and performs a cold reboot. The selected mode persists across all subsequent power cycles until changed again.

#### wifi_mode command reference

```
uart:~$ wifi_mode
Current mode: SoftAP
Usage: wifi_mode [SoftAP|STA|P2P]
  SoftAP  (creates own SoftAP, IP 192.168.7.1)
  STA     (connects to existing Wi-Fi)
  P2P     (Wi-Fi Direct, requires -S wifi-p2p build)
Board reboots automatically after mode change.
```

### STA Connection (per session)

Connect to your router after the board boots in STA mode:

```
uart:~$ wifi scan
uart:~$ wifi connect "MyNetworkSSID" "MyPassword"
```

The dashboard becomes available at `http://<DHCP-IP>` once the device gets an IP from your router.

> **Note:** `wifi connect` connects for the current session only — no credentials are stored. This intentionally prevents auto-connection from triggering in SoftAP or P2P mode if you switch modes later. If you want to check whether a connection is active: `wifi status`.

## 🧭 Workspace Application Setup

This repo is a **workspace application** as described in Nordic's guide ([Creating an application → Workspace application](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/app_dev/create_application.html#workspace_application)). Everything you need is already declared in [`west.yml`](west.yml):

```yaml
manifest:
  version: 0.13
  defaults:
    remote: ncs
  remotes:
    - name: ncs
      url-base: https://github.com/nrfconnect
  projects:
    - name: nrf
      revision: v3.2.4
      import: true
  self:
    path: nordic_wifi_softap_webserver
```

**Quick start:**

1. Follow the Nordic docs section above for the detailed workflow (VS Code or CLI).
2. When the guide tells you to provide a manifest, point it to this project's `west.yml`.
3. Run `west update`, then build and flash as usual (`west build …`, `west flash`).

That's it—by referencing the official instructions you get a reproducible workspace tied to the NCS version pinned in the manifest.

### Connect

1. **Power on** the development kit
2. **Wait ~5 seconds** for WiFi SoftAP to start
3. **Connect your phone/laptop** to WiFi:
   - SSID: `WebDashboard_AP`
   - Password: `12345678`
  - (If you applied the credential overlay, use your custom SSID/password.)
  - **Limit**: Only two stations can be connected at a time; disconnect another client before adding a third.
4. **Open browser** to:
   - `http://192.168.7.1` (static IP)
   - `http://nrfwifi.local` (mDNS — macOS/iOS/Windows; see note below)

## 📡 WiFi Configuration

Default settings (customizable in `prj.conf`):

```properties
CONFIG_APP_WIFI_SSID="WebDashboard_AP"
CONFIG_APP_WIFI_PASSWORD="12345678"
CONFIG_APP_HTTP_PORT=80
```

Static IP configuration:
- **Device IP**: 192.168.7.1
- **Netmask**: 255.255.255.0
- **Gateway**: 192.168.7.1
- **DHCP Server**: Enabled with exactly two leases (192.168.7.2 – 192.168.7.3)
- **Client Ceiling**: WiFi + HTTP layers enforce max 2 stations (each expected to run one browser session)
- **mDNS Hostname**: `nrfwifi.local` — all three modes (SoftAP, STA, P2P)

> **mDNS note**: `http://nrfwifi.local` works on macOS, iOS, and Windows (Bonjour) in all modes. Android lacks native mDNS support — use the IP address shown in the device logs instead (`192.168.7.1` for SoftAP, DHCP-assigned for STA/P2P).

### 🔒 Security Note

**⚠️ IMPORTANT**: The default WiFi password `"12345678"` is for **demonstration purposes only**.

**For production use:**
1. Copy the credential template and keep the real overlay out of version control:
  ```bash
  cp overlay-wifi-credentials.conf.template overlay-wifi-credentials.conf
  ```

2. Edit `overlay-wifi-credentials.conf` with your SSID/password (the filename is already listed in `.gitignore`).

3. Build with the credential overlay applied:
  ```bash
  west build -p -b nrf7002dk/nrf5340/cpuapp -- \
    -DEXTRA_CONF_FILE=overlay-wifi-credentials.conf
  ```

**Password Requirements:**
- Minimum 8 characters (WPA2-PSK requirement)
- Recommended: 12+ characters with mixed case, numbers, and symbols
- Never commit production credentials to version control
  (only `overlay-wifi-credentials.conf.template` is tracked by git)

## 🖥️ Web Interface

![Web Interface](picture/webgui.png)

### Button Status Panel

Displays real-time status for all buttons (2 on nRF7002DK, 3 on nRF54LM20DK+nRF7002EBII):
- Current state (Pressed/Released)
- Total press count
- Visual indicator with animation

### LED Control Panel

Individual control for LEDs (2 on nRF7002DK, 4 on nRF54LM20DK+nRF7002EBII):
- **ON** - Turn LED on
- **OFF** - Turn LED off
- **Toggle** - Switch state
- Visual indicator shows current state

### System Information

- WiFi SSID
- IP Address
- Connection status
- Auto-refresh rate

## 🔌 REST API

All API endpoints support JSON.

### GET /api/buttons

Get current button states.

**Response:**
```json
{
  "buttons": [
    {"number": 0, "name": "Button 1", "pressed": false, "count": 5},
    {"number": 1, "name": "Button 2", "pressed": true, "count": 12}
  ]
}
```
Names adjust automatically based on the connected board (for example, nRF54LM20DK+nRF7002EBII adds a third entry for BUTTON2).

### GET /api/leds

Get current LED states.

**Response:**
```json
{
  "leds": [
    {"number": 0, "name": "LED1", "is_on": true},
    {"number": 1, "name": "LED2", "is_on": false}
  ]
}
```
Additional LED entries automatically appear on platforms with more GPIOs (e.g., four LED objects on nRF54LM20DK+nRF7002EBII).

### POST /api/led

Control an LED.

**Request:**
```json
{
  "led": 1,
  "action": "on"
}
```

**Actions:** `"on"`, `"off"`, `"toggle"`

## 🔧 Customization

### Change Default WiFi Credentials

Edit `prj.conf`:
```properties
CONFIG_APP_WIFI_SSID="YourSSID"
CONFIG_APP_WIFI_PASSWORD="YourPassword"
```

### Change HTTP Port

Edit `prj.conf`:
```properties
CONFIG_APP_HTTP_PORT=8080
```

### Adjust Refresh Rate

Edit `www/main.js`:
```javascript
const REFRESH_INTERVAL = 1000; // Change to 1 second
```

### Modify Hostname

Edit `prj.conf`:
```properties
CONFIG_NET_HOSTNAME="nrfwifi"
```

Then access via `http://nrfwifi.local` (default) or your custom hostname.

## 📊 Memory Usage

Approximate memory footprint:

| Component | Flash | RAM |
|-----------|-------|-----|
| WiFi Stack | ~60 KB | ~50 KB |
| HTTP Server | ~25 KB | ~20 KB |
| SMF/Zbus | ~10 KB | ~5 KB |
| Application | ~15 KB | ~10 KB |
| **Total (static)** | **~110 KB** | **~85 KB** |

Heap budget (dynamic allocations):
- Base heap (`CONFIG_HEAP_MEM_POOL_SIZE`): **80 KB**
- Automatic additions (WPA supplicant, POSIX sockets, Zbus): **~45.7 KB**
- **Managed heap total**: ≈ **125 KB** with warning raised at **88%** high-water mark

### Heap Monitor

`CONFIG_APP_HEAP_MONITOR` keeps an eye on `_system_heap` via Zephyr's heap listener so you can shrink or grow the base safely:

```text
[00:00:03.292,000] <inf> app_heap_monitor: Heap alloc: peak=86016 bytes (68% of 125440), used=65536, free=59904
[00:00:07.914,000] <wrn> app_heap_monitor: Heap alloc: peak=111104 bytes (88% of 125440), used=98304, free=27136
```

- If warnings appear regularly, either bump `CONFIG_HEAP_MEM_POOL_SIZE` or trim dynamic allocations (HTTP buffers, JSON payloads, etc.).
- Tweak `CONFIG_APP_HEAP_MONITOR_WARN_PCT` or `CONFIG_APP_HEAP_MONITOR_STEP_BYTES` to match your product's appetite.

## 🐛 Troubleshooting

### WiFi SoftAP not starting

1. Check logs: `west build -t menuconfig` → Enable detailed WiFi logging
2. Verify board has nRF70 series WiFi chip
3. Ensure firmware is flashed correctly

### Cannot connect to WiFi

1. Verify SSID and password in configuration
2. Check that WiFi is in SoftAP mode (not station mode)
3. Try forgetting and reconnecting to the network

### Web interface not loading

1. Verify IP address is 192.168.7.1
2. Check HTTP server is running (check logs)
3. Ensure firewall is not blocking connection
4. Try different browser

### Buttons not responding

1. Check GPIO configuration for your board
2. Verify DK library is enabled
3. Check button wiring (if using custom board)

### LED control not working

1. Verify LEDs are properly configured in device tree
2. Check DK library initialization
3. Verify LED Zbus channel is working

## 📝 Development Notes

### Adding New Modules

1. Create new directory under `src/modules/`
2. Add SMF states and Zbus channels
3. Define module in `CMakeLists.txt`
4. Add Kconfig options
5. Update `messages.h` with new message types

### Debugging

Enable detailed logging in `prj.conf`:
```properties
CONFIG_LOG_MODE_IMMEDIATE=y
CONFIG_BUTTON_MODULE_LOG_LEVEL_DBG=y
CONFIG_LED_MODULE_LOG_LEVEL_DBG=y
CONFIG_WIFI_MODULE_LOG_LEVEL_DBG=y
CONFIG_WEBSERVER_MODULE_LOG_LEVEL_DBG=y
```

### Thread Stack Analysis

Enable thread analyzer in `prj.conf`:
```properties
CONFIG_THREAD_ANALYZER=y
CONFIG_THREAD_ANALYZER_USE_LOG=y
CONFIG_THREAD_ANALYZER_AUTO=y
CONFIG_THREAD_ANALYZER_AUTO_INTERVAL=5
```

## 📚 References

- [nRF Connect SDK Documentation](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html)
- [Zephyr State Machine Framework](https://docs.zephyrproject.org/latest/services/smf/index.html)
- [Zephyr Zbus](https://docs.zephyrproject.org/latest/services/zbus/index.html)
- [nRF70 Series WiFi](https://www.nordicsemi.com/Products/nRF7002)

## 🤝 Contributing

This project follows Nordic Semiconductor coding standards. Contributions welcome!

## 📄 License

Copyright (c) 2026 Nordic Semiconductor ASA

SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

## 🤖 Development

This project was developed using [Charlie Skills](https://github.com/chshzh/charlie-skills) with Product Manager and Developer roles for systematic requirements management, architecture design, and implementation.

---

**Happy coding!** 🚀
