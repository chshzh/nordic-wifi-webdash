# Nordic Wi-Fi WebDash

[![Build](https://github.com/chshzh/nordic-wifi-webdash/actions/workflows/build.yml/badge.svg)](https://github.com/chshzh/nordic-wifi-webdash/actions/workflows/build.yml)
[![License](https://img.shields.io/badge/License-LicenseRef--Nordic--5--Clause-blue.svg)](LICENSE)

Nordic Wi-Fi WebDash is a browser-based demo and reference application for Nordic nRF7x Wi-Fi development kits. The device hosts the dashboard itself, so users can monitor buttons, control LEDs, and inspect system state directly from a browser without relying on cloud services.

The firmware supports **four** Wi-Fi operating modes: SoftAP, STA, P2P_GO, and P2P_CLIENT. The selected mode is stored in NVS and can be changed at runtime with the `app_wifi_mode` shell command. **Default on fresh flash is P2P_GO.**

Connect to the device via Wi-Fi and open the dashboard in your browser:

![Web Interface](picture/webgui.png)

## Project Overview

### Introduction

This project is designed for two common use cases:

- **Evaluator** — grab a pre-built `.hex` from the [Releases](https://github.com/chshzh/nordic-wifi-webdash/releases) page, flash it, and follow the [Quick Start](#quick-start) guide to reach the dashboard in two steps.
- **Developer** — clone the workspace, build from source, and customise the firmware; see [Developer Info](#developer-info) for build setup and [Documentation](#documentation) for product requirements, architecture, and per-module specs.

Supported hardware:

- nRF7002DK (`nrf7002dk/nrf5340/cpuapp`)
- nRF54LM20DK + nRF7002EBII (`nrf54lm20dk/nrf54lm20a/cpuapp` + shield)

### Features

- Four Wi-Fi modes: SoftAP, STA, P2P_GO (device is Group Owner), and P2P_CLIENT (device joins phone's group)
- Runtime mode switching with `app_wifi_mode [softap|sta|p2p_go|p2p_client]`
- Browser dashboard for button status, LED control, and system information
- REST API for `/api/system`, `/api/buttons`, `/api/leds`, and `/api/led`
- Gzip-compressed static web assets served from flash
- mDNS hostname support via `http://nrfwebdash.local`
- Modular architecture based on SMF + Zbus
- Startup banner with firmware version string (git tag on CI / `v<NCS>-dev` locally), module boot sequence with SYS_INIT priorities, and periodic reminders (SSID/PIN) until a client connects

### Project Structure

```text
nordic-wifi-webdash/
├── CMakeLists.txt
├── Kconfig
├── prj.conf
├── west.yml
├── docs/
│   ├── PRD.md                     ← product requirements, features, acceptance criteria
│   └── specs/
│       ├── overview.md            ← spec index, PRD-to-spec mapping, architecture summary
│       ├── architecture.md        ← module map, Zbus channels, SYS_INIT boot order
│       ├── network-module.md      ← SoftAP / STA / P2P_GO / P2P_CLIENT paths
│       ├── mode-selector.md       ← app_wifi_mode shell command, NVS persistence
│       ├── webserver-module.md    ← HTTP server, REST API, DNS-SD, web UI
│       ├── button-module.md       ← GPIO button monitoring, SMF events
│       └── led-module.md          ← LED control, Zbus-commanded
├── src/
│   ├── main.c
│   └── modules/
│       ├── button/
│       ├── led/
│       ├── memory/
│       ├── mode_selector/
│       ├── network/
│       ├── webserver/
│       └── messages.h
```

## Quick Start

This section is intentionally short so an evaluator can get to a working dashboard quickly.

### Step 1 - Flash the firmware

Download the pre-built `.hex` for your board from the [Releases](https://github.com/chshzh/nordic-wifi-webdash/releases) page, then open **nRF Connect for Desktop -> Programmer**, select your board, add the `.hex` file, and click **Erase & Write**.

| Board | Release page |
|-------|--------------|
| nRF7002DK | [Latest release](https://github.com/chshzh/nordic-wifi-webdash/releases/latest) |
| nRF54LM20DK + nRF7002EBII | [Latest release](https://github.com/chshzh/nordic-wifi-webdash/releases/latest) |

### Step 2 - Choose Wi-Fi mode and open the dashboard

Open a serial terminal at `115200` baud and follow the instructions printed by the firmware.

- **P2P_GO** (default on fresh flash): the P2P group and WPS PIN (`12345678`) are auto-started at boot — no shell commands needed; on the phone open Wi-Fi Direct, wait for the DK to appear, select it, enter PIN `12345678`, then open `http://192.168.7.1` or `http://nrfwebdash.local`
- **P2P_CLIENT**: run `app_wifi_mode P2P_CLIENT`, reboot, the device auto-starts peer discovery; on the phone enable Wi-Fi Direct, then run `wifi p2p connect <MAC> pbc` on the device and accept on the phone; open the IP shown in the terminal
- **SoftAP**: run `app_wifi_mode SoftAP`, reboot, connect to Wi-Fi `WebDash_AP` with password `12345678`, then open `http://192.168.7.1`
- **STA**: run `app_wifi_mode STA`, reboot, then run `wifi connect -s <SSID> -p <password> -k 1` and open the `http://<DHCP-IP>` shown in the terminal

At any time, you can switch modes with `uart:~$ app_wifi_mode [softap|sta|p2p_go|p2p_client]`. The choice is saved to NVS and survives reboot.

## Developer Info

This section covers environment setup, build commands, and configuration. For product requirements, architecture decisions, and per-module specs, see the [Documentation](#documentation) section below.

### Environment Setup

- nRF Connect SDK `v3.3.0`
- West workspace driven by [west.yml](west.yml)
- nRF Connect for VS Code or a shell initialized with the NCS toolchain

### Build From Source

```bash
# nRF7002DK
west build -p -b nrf7002dk/nrf5340/cpuapp -- -DSNIPPET=wifi-p2p

# nRF54LM20DK + nRF7002EBII
west build -p -b nrf54lm20dk/nrf54lm20a/cpuapp -- -DSNIPPET=wifi-p2p -DSHIELD=nrf7002eb2
```

Flash with:

```bash
west flash
```

### Workspace Setup

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

See the Nordic guide on [Workspace Application Setup](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/dev_model_and_contributions/adding_code.html#workflow_4_workspace_application_repository_recommended).

```sh
west init -m https://github.com/chshzh/nordic-wifi-webdash --mr main <workspace-dir>
cd <workspace-dir>
west update
```

For product context and implementation details, start at [docs/specs/overview.md](docs/specs/overview.md) — it maps every PRD requirement to the spec file that implements it.

### Configuration

Key application settings are in [prj.conf](prj.conf):

```properties
CONFIG_APP_WIFI_SSID="WebDash_AP"
CONFIG_APP_WIFI_PASSWORD="12345678"
CONFIG_APP_HTTP_PORT=80
CONFIG_NET_HOSTNAME="nrfwebdash"
```

### Developer Notes

- nRF54LM20DK + nRF7002EBII loses one button because of shield pin conflicts; BUTTON0–BUTTON2 remain available
- STA connections are intentionally session-based to avoid unwanted reconnects when returning to other modes
- Default mode on fresh flash is P2P_GO — switch to SoftAP or STA with `app_wifi_mode` if preferred
- The startup banner prints firmware version (`Version: <tag>` on CI / `v<NCS>-dev` locally), aligned board/MAC/mode labels, and a module list with SYS_INIT boot priorities — useful for orientation when reading serial logs
- SoftAP and P2P_GO both log connectivity instructions every 300 s until the first client connects; the reminders stop automatically on first connection
- mDNS behavior and module responsibilities are in [docs/specs/network-module.md](docs/specs/network-module.md); mode handling is in [docs/specs/mode-selector.md](docs/specs/mode-selector.md)

### Troubleshooting

- SoftAP not reachable: verify the terminal shows the expected IP and SoftAP instructions
- STA not reachable: confirm the device received a DHCP IP and use that address first
- mDNS not resolving: test the printed IP before investigating hostname resolution on the host OS
- Build issues: confirm the workspace is using NCS `v3.3.0` and the correct board/shield combination
- P2P_GO not working: ensure build uses `-DSNIPPET=wifi-p2p`; group is auto-started at boot — check the terminal for errors if it fails
- P2P_CLIENT not finding peers: verify `-DSNIPPET=wifi-p2p` is used; phone's Wi-Fi Direct must be active

## Web Interface

The firmware serves a live dashboard. Connect to the device via Wi-Fi and open the printed IP (or `http://nrfwebdash.local`) in any browser.

### Button Status Panel

Displays real-time status for all available buttons.

- Current state (Pressed/Released)
- Press count
- Board-aware button naming and count

### LED Control Panel

Provides per-LED control for supported boards.

- `ON`
- `OFF`
- `Toggle`

### System Information

The dashboard also reports:

- Active Wi-Fi mode and SSID
- Device IP address and MAC
- Connected client IP
- Uptime

## Documentation

| Document | Description |
|---|---|
| [docs/PRD.md](docs/PRD.md) | Product Requirements — features, behavior, acceptance criteria, changelog |
| [docs/specs/overview.md](docs/specs/overview.md) | **Start here** — spec index, PRD-to-spec mapping, architecture summary, design decisions |
| [docs/specs/architecture.md](docs/specs/architecture.md) | System architecture — module map, Zbus channels, SYS_INIT boot sequence, memory budget |
| [docs/specs/network-module.md](docs/specs/network-module.md) | Network module — SoftAP / STA / P2P_GO / P2P_CLIENT paths, event handling, WPS |
| [docs/specs/mode-selector.md](docs/specs/mode-selector.md) | Mode selector — `app_wifi_mode` shell command, NVS persistence, factory default |
| [docs/specs/webserver-module.md](docs/specs/webserver-module.md) | Webserver module — HTTP server, REST API endpoints, DNS-SD, web UI |
| [docs/specs/button-module.md](docs/specs/button-module.md) | Button module — GPIO monitoring, SMF press/release events, board differences |
| [docs/specs/led-module.md](docs/specs/led-module.md) | LED module — per-LED state machine, Zbus-commanded via `LED_CMD_CHAN` |

The PRD describes **what** the device should do and for whom. The engineering specs describe **how** — each module spec maps back to PRD requirements and documents the state machine, Kconfig options, API, and test cases for that module.

## REST API

All API endpoints use JSON.

### GET /api/system

Returns active mode, device IP, device MAC, connected HTTP client IP, SSID, uptime, and board name.

Example response:
```json
{
  "mode": "P2P_GO",
  "device_ip": "192.168.49.1",
  "device_mac": "AA:BB:CC:DD:EE:FF",
  "client_ip": "192.168.49.100",
  "ssid": "DIRECT-xx-WebDash",
  "uptime_s": 120,
  "board": "nrf54lm20dk"
}
```

### GET /api/buttons

Returns current button states and press counts.

Example response:

```json
{
  "buttons": [
    {"number": 0, "name": "Button 1", "pressed": false, "count": 5},
    {"number": 1, "name": "Button 2", "pressed": true, "count": 12}
  ]
}
```

### GET /api/leds

Returns current LED states.

Example response:

```json
{
  "leds": [
    {"number": 0, "name": "LED1", "is_on": true},
    {"number": 1, "name": "LED2", "is_on": false}
  ]
}
```

### POST /api/led

Controls a single LED.

Example request:

```json
{
  "led": 1,
  "action": "on"
}
```

Supported actions: `on`, `off`, `toggle`

## Development Methodology

This project was developed using the [chsh-ncs-workflow](https://github.com/chshzh/charlie-skills) — a four-phase lifecycle for NCS/Zephyr IoT projects where each phase has a dedicated AI skill:

| Phase | Focus | Skill | Output |
|-------|-------|-------|--------|
| 1 — Product Definition | What the device should do, for whom, and why | `chsh-pm-prd` | `docs/PRD.md` |
| 2 — Technical Design | Translate PRD into engineering specs | `chsh-dev-spec` | `docs/specs/*.md` |
| 3 — Implementation | Implement code from approved specs | `chsh-dev-project` | `src/`, passing build |
| 4 — QA & Test | Validate the build against PRD criteria | `chsh-qa-test` | `TEST-*.md`, `QA-*.md` |

Each phase feeds the next: requirements drive specs, specs drive code, code drives tests. Issues loop back to the right phase — code bugs to Phase 3, spec gaps to Phase 2, new requirements to Phase 1.

Supporting skills: `chsh-dev-commit` (logical git history), `chsh-dev-mem-opt` (flash/RAM analysis).

## References

- [nRF Connect SDK Documentation](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html)
- [Zephyr State Machine Framework](https://docs.zephyrproject.org/latest/services/smf/index.html)
- [Zephyr Zbus](https://docs.zephyrproject.org/latest/services/zbus/index.html)
- [nRF70 Series Wi-Fi](https://www.nordicsemi.com/Products/nRF7002)

## Contributing

This project follows Nordic Semiconductor coding standards. Contributions are welcome.

## License

Copyright (c) 2026 Nordic Semiconductor ASA

SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


