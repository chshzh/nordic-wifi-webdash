# Nordic Wi-Fi WebDash

[![Validation](https://github.com/chshzh/nordic-wifi-webdash/actions/workflows/validation.yml/badge.svg)](https://github.com/chshzh/nordic-wifi-webdash/actions/workflows/validation.yml)
[![Latest Release](https://img.shields.io/github/v/release/chshzh/nordic-wifi-webdash?label=Latest%20Release&color=skyblue)](https://github.com/chshzh/nordic-wifi-webdash/releases/latest)
---

## Project Overview

### Introduction

Nordic Wi-Fi WebDash is a browser-based demo and reference application for Nordic nRF7x Wi-Fi development kits. The device hosts the dashboard itself, so users can monitor buttons, control LEDs, and inspect system state directly from a browser without relying on cloud services.

The firmware supports **four** Wi-Fi operating modes: SoftAP, STA, P2P_GO, and P2P_CLIENT. The selected mode is stored in NVS and can be changed at runtime with the `app_wifi_mode` shell command. **Default on fresh flash is STA.**

### Supported hardware

| Board | Build target |
|-------|--------------|
| nRF54LM20DK + nRF7002EB2 | `nrf54lm20dk/nrf54lm20a/cpuapp` + `-DSHIELD=nrf7002eb2` |
| nRF7002DK | `nrf7002dk/nrf5340/cpuapp` |

### Features

- Four Wi-Fi modes: SoftAP, STA, P2P_GO (device is Group Owner), and P2P_CLIENT (device joins phone's group)
- Runtime mode switching with `app_wifi_mode [softap|sta|p2p_go|p2p_client]`
- Browser dashboard for button status, LED control, and system information
- REST API for `/api/system`, `/api/buttons`, `/api/leds`, and `/api/led`
- Gzip-compressed static web assets served from flash
- mDNS hostname support via `http://nrfwebdash.local`
- Modular architecture based on Zbus; button, LED, Wi-Fi mode-selector, and network event handling provided by standalone **[zego](../zego)** library modules (`zego/button`, `zego/led`, `zego/wifi`, `zego/network`)
- Light/dark mode: auto-detects `prefers-color-scheme` with a manual toggle in the header (resets on reload, no persistence)
- Startup banner with firmware version string (git tag on CI / `v<NCS>-dev` locally), module boot sequence with SYS_INIT priorities, and periodic reminders (SSID/PIN) until a client connects

### Target Users

- **Evaluator** — grab a pre-built `.hex` from the [Releases](https://github.com/chshzh/nordic-wifi-webdash/releases) page, flash it, and follow the [Evaluator Quick Start](#evaluator-quick-start) guide to reach the dashboard in under 5 minutes.
- **Developer** — clone the workspace, build from source, and customise the firmware; see [Developer Guide](#developer-guide) for build setup and [Documentation](#documentation) for product requirements, architecture, and per-module specs.

---

## Evaluator Quick Start

> Evaluator path — no build environment needed. ~5 minutes.

### Step 1 — Flash the firmware

Download the pre-built `.hex` for your board from the [Releases](https://github.com/chshzh/nordic-wifi-webdash/releases) page, then open **nRF Connect for Desktop -> Programmer**, select your board, add the `.hex` file, and click **Erase & Write**.

| Board | Release page |
|-------|--------------|
| nRF54LM20DK + nRF7002EB2 | [Latest release](https://github.com/chshzh/nordic-wifi-webdash/releases/latest) |
| nRF7002DK | [Latest release](https://github.com/chshzh/nordic-wifi-webdash/releases/latest) |

### Step 2 — Connect and open the dashboard

Open a serial terminal at `115200` baud and follow the instructions printed by the firmware.

- **P2P_GO** (default on fresh flash): the P2P group and WPS PIN (`12345678`) are auto-started at boot — no shell commands needed; on the phone open Wi-Fi Direct, wait for the DK to appear, select it, enter PIN `12345678`, then open `http://192.168.7.1` or `http://nrfwebdash.local`
- **P2P_CLIENT**: run `app_wifi_mode P2P_CLIENT`, reboot, the device auto-starts peer discovery; on the phone enable Wi-Fi Direct, then run `wifi p2p connect <MAC> pbc` on the device and accept on the phone; open the IP shown in the terminal
- **SoftAP**: run `app_wifi_mode SoftAP`, reboot, connect to Wi-Fi `WebDash_AP` with password `12345678`, then open `http://192.168.7.1`
- **STA**: run `app_wifi_mode STA`, reboot, then run `wifi connect -s <SSID> -p <password> -k 1` and open the `http://<DHCP-IP>` shown in the terminal

At any time, you can switch modes with `uart:~$ app_wifi_mode [softap|sta|p2p_go|p2p_client]`. The choice is saved to NVS and survives reboot.

### Step 3 — Verify

Open `http://192.168.7.1` (P2P_GO or SoftAP) or the IP printed in the terminal (STA or P2P_CLIENT) in any browser. The live dashboard shows button states, LED controls, and system information. `http://nrfwebdash.local` also works on hosts that support mDNS.

![WebDash dashboard](picture/webgui.png)

## Buttons & LEDs

### Buttons

| Board | Buttons | Function |
|-------|---------|----------|
| nRF54LM20DK + nRF7002EB2 | BUTTON0, BUTTON1, BUTTON2 | Same (BUTTON3 unavailable — shield pin conflict) |
| nRF7002DK | Button 1, Button 2 | State and press count shown in dashboard and reported via `/api/buttons` |

### LEDs

| Board | LEDs | Control |
|-------|------|---------|
| nRF54LM20DK | LED0, LED1, LED2, LED3 | Same |
| nRF7002DK | LED1, LED2 | Controlled via dashboard (`on` / `off` / `toggle`) or `/api/led` |

---

## Developer Guide

### Project Structure

```text
nordic-wifi-webdash/
├── CMakeLists.txt          ← registers zego/button + zego/led + zego/wifi via EXTRA_ZEPHYR_MODULES
├── Kconfig
├── prj.conf
├── west.yml
├── boards/                 ← per-board Kconfig fragments (button count, LED count)
├── docs/
│   ├── pm-prd/
│   │   └── PRD.md                 ← product requirements, features, acceptance criteria
│   ├── dev-specs/
│   │   ├── overview.md            ← spec index, PRD-to-spec mapping, architecture summary
│   │   ├── architecture.md        ← module map, Zbus channels, SYS_INIT boot order
│   │   ├── network-module.md      ← net_event_app.c shim — weak-hook overrides of zego/network
│   │   └── webserver-module.md    ← HTTP server, REST API, DNS-SD, web UI
│   └── qa-test/
│       └── QA-*.md               ← dated test + QA reports
├── src/
│   ├── main.c
│   └── modules/
│       ├── memory/
│       ├── network/              ← net_event_app.c only (zego/network provides the backbone)
│       ├── webserver/
│       └── messages.h
└── ../zego/                ← sibling repo — external Zephyr modules
    ├── button/             ← zego/button: gesture detection, BUTTON_CHAN
    ├── led/                ← zego/led: static/blink/breathe/rotate, LED_CMD_CHAN
    ├── wifi/               ← zego/wifi: mode selector, NVS, app_wifi_mode shell cmd
    └── network/            ← zego/network: Wi-Fi event management, weak-hook API
```


### Workspace Setup

West workspace is driven by [west.yml](west.yml). Which contains the ncs version this application based on, for example, the following content means ncs v3.3.0.
```sh
    - name: sdk-nrf
      path: nrf
      revision: v3.3.0
      import: true
      remote: ncs
```

Release versions follow the NCS version with a build counter suffix: `v<ncs-version>.<build>` (e.g. `v3.3.0.1`, `v3.3.0.2`). The major/minor/patch components always match the NCS version the firmware is based on, making it easy to identify which SDK a given release targets.

Use nRF Connect for VS Code or a shell initialized with the NCS toolchain.

#### Method 1 (Preferred) — Add to an existing NCS installation

If you already have a matching NCS version installed, reuse it directly — no re-downloading required.

Under a terminal with the toolchain:

```sh
cd /opt/nordic/ncs/<ncs-version>   # your existing NCS workspace root

git clone https://github.com/chshzh/nordic-wifi-webdash.git

# Switch the workspace manifest to nordic-wifi-webdash (one-time change)
west config manifest.path nordic-wifi-webdash

# Sync — NCS repos already present, only new project repos are cloned
west update
```

#### Method 2 — Fresh installation as a Workspace Application

##### Option A: nRF Connect for VS Code

Follow the [custom repository guide](https://docs.nordicsemi.com/bundle/nrf-connect-vscode/page/guides/extension_custom_repo.html).

##### Option B: CLI

```sh
west init -m https://github.com/chshzh/nordic-wifi-webdash --mr main <workspace-dir>
cd <workspace-dir>
west update
```

See the Nordic guide on [Workspace Application Setup](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/dev_model_and_contributions/adding_code.html#workflow_4_workspace_application_repository_recommended) for details.

### Build

```bash
# nRF54LM20DK + nRF7002EB2
west build -p -b nrf54lm20dk/nrf54lm20a/cpuapp -d build_nrf54lm20dk  -- -Dnordic-wifi-webdash_SNIPPET=wifi-p2p -DSHIELD=nrf7002eb2

# nRF7002DK
west build -p -b nrf7002dk/nrf5340/cpuapp -d build_nrf7002dk  -- -Dnordic-wifi-webdash_SNIPPET=wifi-p2p
```

### Feature Overlay Builds

Two optional overlay conf files are provided for minimal / STA-only builds:

| Overlay | Purpose |
|---------|---------|
| `overlay-sta-webserver.conf` | STA-only mode, HTTP server enabled. Disables SoftAP driver and DHCP server — use when you only need STA + browser dashboard. |
| `overlay-sta-no-webserver.conf` | STA-only mode, HTTP server **disabled**. Strips the entire HTTP stack, mDNS, and DNS-SD from the binary. Use for headless STA applications or memory comparison builds. |

Pass a single overlay:
```bash
# nRF7002DK — STA-only with webserver
west build -p -b nrf7002dk/nrf5340/cpuapp -d build_sta_webserver -- \
  -DEXTRA_CONF_FILE="overlay-sta-webserver.conf"

# nRF7002DK — STA-only, no webserver
west build -p -b nrf7002dk/nrf5340/cpuapp -d build_sta_no_webserver -- \
  -DEXTRA_CONF_FILE="overlay-sta-no-webserver.conf"
```

Measured memory delta on nRF7002DK (NCS v3.3.0, `arm-zephyr-eabi-size`):

| Config | Flash | RAM (448 KB app core) |
|--------|-------|-----------------------|
| STA + webserver | 687 KB (67.1%) | 381 KB (85.0%) |
| STA, no webserver | 642 KB (62.6%) | 362 KB (80.9%) |
| **Webserver cost** | **+44.5 KB** | **+18.6 KB** |

> The `CONFIG_WEBSERVER_MODULE=n` Kconfig now properly gates `webserver.c` compilation and
> all HTTP linker sections, so setting it to `n` results in zero HTTP overhead.

### Flash

**First-time flash** (erases all flash including NVS — Wi-Fi credentials will need to be re-provisioned):

```bash
# nRF54LM20DK
west flash -d build_nrf54lm20dk --recover

# nRF7002DK
west flash -d build_nrf7002dk --erase
```

### Developer Notes

- nRF54LM20DK + nRF7002EB2 loses one button because of shield pin conflicts; BUTTON0–BUTTON2 remain available
- STA connections are intentionally session-based to avoid unwanted reconnects when returning to other modes
- Default mode on fresh flash is P2P_GO — switch to SoftAP or STA with `app_wifi_mode` if preferred
- The startup banner prints firmware version (`Version: <tag>` on CI / `v<NCS>-dev` locally), aligned board/MAC/mode labels, and a module list with SYS_INIT boot priorities — useful for orientation when reading serial logs
- SoftAP and P2P_GO both log connectivity instructions every 300 s until the first client connects; the reminders stop automatically on first connection
- mDNS behavior and network event details are in [zego/network ↗](https://github.com/chshzh/zego/blob/main/bricks/network/docs/network-spec.md) and [docs/dev-specs/network-module.md](docs/dev-specs/network-module.md); mode handling is in [zego/wifi ↗](https://github.com/chshzh/zego/blob/main/bricks/wifi/docs/wifi-spec.md)

### Live Memory and Thread Monitoring with ZView

Run ZView from the project root while the board is connected over J-Link. Replace the `-s` serial with your board's J-Link serial number (`nrfjprog --ids`).

**nRF54LM20DK + nRF7002EB2:**
```bash
west zview live \
  -e build_nrf54lm20dk/nordic-wifi-webdash/zephyr/zephyr.elf \
  -r jlink \
  -t nRF54LM20A_M33 \
  -s 1051869687(replace with target <jlink-serial>)
```

**nRF7002DK:**
```bash
west zview live \
  -e build_nrf7002dk/nordic-wifi-webdash/zephyr/zephyr.elf \
  -r jlink \
  -t nRF5340_xxAA \
  -s 1050787962(replace with target <jlink-serial>)
```

The in-browser Thread Monitor and Heap Monitor panels update at the same interval as `CONFIG_ZEGO_MEMONITOR_INTERVAL_MS` (default 5 s) using `/api/threads` and `/api/heaps`.

#### Memory Sizing Rules

All watermarks (HWM) are **peak values** accumulated since boot — read them after exercising all four Wi-Fi modes in one session for worst-case coverage.

**Thread stacks:**
- Resize if HWM > **80%** of allocated stack size.
- For large, well-characterised stacks (> 2048 B), the practical threshold is **90%** — `hostap_handler`, `hostap_iface_wq`, and `nrf70_bh_wq` routinely sit at 85–90 % and are stable.
- Sizing formula: `CONFIG_<THREAD>_STACK_SIZE = HWM / 0.9` (gives ≈ 10 % headroom).

**System heap:**
- Resize if heap HWM > **80%** of total heap size.
- Sizing formula: `CONFIG_HEAP_MEM_POOL_SIZE = HWM / 0.8` (gives ≈ 20 % headroom).

The in-browser HWM % bars turn amber at 80 % and red at 90 % (large stacks only) to flag threads that need attention.


## Documentation

The full design documentation lives under `docs/`. Start with [docs/dev-specs/overview.md](docs/dev-specs/overview.md), which maps every PRD requirement to the spec file that implements it and provides an architecture summary.

| Document | Description |
|---|---|
| [docs/pm-prd/PRD.md](docs/pm-prd/PRD.md) | Product Requirements — user perspective features, behavior, acceptance criteria, changelog |
| [docs/dev-specs/overview.md](docs/dev-specs/overview.md) | **Start here** — technical spec index, PRD-to-spec mapping, architecture summary, design decisions |
| [docs/dev-specs/architecture.md](docs/dev-specs/architecture.md) | System architecture — module map, Zbus channels, SYS_INIT boot sequence, memory budget |
| [docs/dev-specs/network-module.md](docs/dev-specs/network-module.md) | Network module — `net_event_app.c` shim, `CLIENT_CONNECTED_CHAN`, weak-hook overrides of zego/network |
| [docs/dev-specs/webserver-module.md](docs/dev-specs/webserver-module.md) | Webserver module — HTTP server, REST API endpoints, DNS-SD, web UI |
| [zego/wifi ↗](https://github.com/chshzh/zego/blob/main/bricks/wifi/docs/wifi-spec.md) | Mode selector — `app_wifi_mode` shell command, NVS persistence, factory default |
| [zego/network ↗](https://github.com/chshzh/zego/blob/main/bricks/network/docs/network-spec.md) | Network backbone — SoftAP / STA / P2P_GO / P2P_CLIENT paths, event handling, WPS |
| [zego/button ↗](https://github.com/chshzh/zego/blob/main/bricks/button/docs/button-spec.md) | Button module — gesture detection (click, double-click, long press), Zbus `BUTTON_CHAN`; provided by **zego/button** |
| [zego/led ↗](https://github.com/chshzh/zego/blob/main/bricks/led/docs/led-spec.md) | LED module — per-LED state machine (static, blink, breathe, rotate), Zbus `LED_CMD_CHAN`; provided by **zego/led** |

## Methodology

This project was developed using the [chsh-sk-ncs-0-workflow skill](https://github.com/chshzh/claude/blob/main/skills/chsh-sk-ncs-0-workflow/SKILL.md) — a four-phase lifecycle for NCS/Zephyr IoT projects where each phase has a dedicated AI skill:

| Phase | Focus | Skill | Output |
|-------|-------|-------|--------|
| 1 — Product Definition | What the device should do, for whom, and why | `chsh-sk-ncs-1-prd` | `docs/pm-prd/PRD.md` |
| 2 — Technical Design | Translate PRD into engineering specs | `chsh-sk-ncs-2-spec` | `docs/dev-specs/*.md` |
| 3 — Implementation | Implement, debug, and optimise code from approved specs | `chsh-sk-ncs-3.1-coding` · `chsh-sk-ncs-3.2-debug` · `chsh-sk-ncs-3.3-memopt` | `src/`, passing build |
| 4 — V&V | Verify code quality (no HW), then validate on hardware against PRD criteria | `chsh-sk-ncs-4.1-verification` · `chsh-sk-ncs-4.2-validation` | `docs/qa-test/VERIFICATION-*.md` + `docs/qa-test/VALIDATION-*.md` |

Each phase feeds the next: requirements drive specs, specs drive code, code drives tests. Issues loop back to the right phase — code bugs to Phase 3, spec gaps to Phase 2, new requirements to Phase 1.


---

## License

[SPDX-License-Identifier: LicenseRef-Nordic-5-Clause](LICENSE)
