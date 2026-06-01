# Nordic Wi-Fi WebDash

[![Build](https://github.com/chshzh/nordic-wifi-webdash/actions/workflows/build.yml/badge.svg)](https://github.com/chshzh/nordic-wifi-webdash/actions/workflows/build.yml)
[![Latest Release](https://img.shields.io/github/v/release/chshzh/nordic-wifi-webdash?label=Release&color=skyblue)](https://github.com/chshzh/nordic-wifi-webdash/releases/latest)
---

## Project Overview

### Introduction

Nordic Wi-Fi WebDash is a browser-based demo and reference application for Nordic nRF7x Wi-Fi development kits. The device hosts the dashboard itself, so users can monitor buttons, control LEDs, and inspect system state directly from a browser without relying on cloud services.

The firmware supports **four** Wi-Fi operating modes: SoftAP, STA, P2P_GO, and P2P_CLIENT. The selected mode is stored in NVS and can be changed at runtime with the `app_wifi_mode` shell command. **Default on fresh flash is P2P_GO.**

### Supported hardware

| Board | Build target |
|-------|--------------|
| nRF7002DK | `nrf7002dk/nrf5340/cpuapp` |
| nRF54LM20DK + nRF7002EBII | `nrf54lm20dk/nrf54lm20a/cpuapp` + `-DSHIELD=nrf7002eb2` |

### Features

- Four Wi-Fi modes: SoftAP, STA, P2P_GO (device is Group Owner), and P2P_CLIENT (device joins phone's group)
- Runtime mode switching with `app_wifi_mode [softap|sta|p2p_go|p2p_client]`
- Browser dashboard for button status, LED control, and system information
- REST API for `/api/system`, `/api/buttons`, `/api/leds`, and `/api/led`
- Gzip-compressed static web assets served from flash
- mDNS hostname support via `http://nrfwebdash.local`
- Modular architecture based on SMF + Zbus
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
| nRF7002DK | [Latest release](https://github.com/chshzh/nordic-wifi-webdash/releases/latest) |
| nRF54LM20DK + nRF7002EBII | [Latest release](https://github.com/chshzh/nordic-wifi-webdash/releases/latest) |

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
| nRF7002DK | Button 1, Button 2 | State and press count shown in dashboard and reported via `/api/buttons` |
| nRF54LM20DK + nRF7002EBII | BUTTON0, BUTTON1, BUTTON2 | Same (BUTTON3 unavailable — shield pin conflict) |

### LEDs

| Board | LEDs | Control |
|-------|------|---------|
| nRF7002DK | LED1, LED2 | Controlled via dashboard (`on` / `off` / `toggle`) or `/api/led` |
| nRF54LM20DK | LED0, LED1, LED2, LED3 | Same |

---

## Developer Guide

### Project Structure

```text
nordic-wifi-webdash/
├── CMakeLists.txt          ← registers zego/button + zego/led via EXTRA_ZEPHYR_MODULES
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
│   │   ├── network-module.md      ← SoftAP / STA / P2P_GO / P2P_CLIENT paths
│   │   ├── mode-selector.md       ← app_wifi_mode shell command, NVS persistence
│   │   └── webserver-module.md    ← HTTP server, REST API, DNS-SD, web UI
│   └── qa-test/
│       └── QA-*.md               ← dated test + QA reports
├── src/
│   ├── main.c
│   └── modules/
│       ├── memory/
│       ├── mode_selector/
│       ├── network/
│       ├── webserver/
│       └── messages.h
└── ../zego/                ← sibling repo — external Zephyr modules
    ├── button/             ← zego/button: gesture detection, BUTTON_CHAN
    └── led/                ← zego/led: static/blink/breathe/marquee, LED_CMD_CHAN
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
# nRF7002DK
west build -p -b nrf7002dk/nrf5340/cpuapp -d build_nrf7002dk  -- -DSNIPPET=wifi-p2p

# nRF54LM20DK + nRF7002EBII
west build -p -b nrf54lm20dk/nrf54lm20a/cpuapp -d build_nrf54lm20dk  -- -DSNIPPET=wifi-p2p -DSHIELD=nrf7002eb2
```

### Flash

**First-time flash** (erases all flash including NVS — Wi-Fi credentials will need to be re-provisioned):

```bash
# nRF7002DK
west flash -d build_nrf7002dk --erase

# nRF54LM20DK
west flash -d build_nrf54lm20dk --recover
```

### Serial Monitor

Connect at **115200 baud**. The device prints its IP address, Wi-Fi mode, and connection instructions at boot.

### Developer Notes

- nRF54LM20DK + nRF7002EBII loses one button because of shield pin conflicts; BUTTON0–BUTTON2 remain available
- STA connections are intentionally session-based to avoid unwanted reconnects when returning to other modes
- Default mode on fresh flash is P2P_GO — switch to SoftAP or STA with `app_wifi_mode` if preferred
- The startup banner prints firmware version (`Version: <tag>` on CI / `v<NCS>-dev` locally), aligned board/MAC/mode labels, and a module list with SYS_INIT boot priorities — useful for orientation when reading serial logs
- SoftAP and P2P_GO both log connectivity instructions every 300 s until the first client connects; the reminders stop automatically on first connection
- mDNS behavior and module responsibilities are in [docs/dev-specs/network-module.md](docs/dev-specs/network-module.md); mode handling is in [docs/dev-specs/mode-selector.md](docs/dev-specs/mode-selector.md)


## Documentation

The full design documentation lives under `docs/`. Start with [docs/dev-specs/overview.md](docs/dev-specs/overview.md), which maps every PRD requirement to the spec file that implements it and provides an architecture summary.

| Document | Description |
|---|---|
| [docs/pm-prd/PRD.md](docs/pm-prd/PRD.md) | Product Requirements — user perspective features, behavior, acceptance criteria, changelog |
| [docs/dev-specs/overview.md](docs/dev-specs/overview.md) | **Start here** — technical spec index, PRD-to-spec mapping, architecture summary, design decisions |
| [docs/dev-specs/architecture.md](docs/dev-specs/architecture.md) | System architecture — module map, Zbus channels, SYS_INIT boot sequence, memory budget |
| [docs/dev-specs/network-module.md](docs/dev-specs/network-module.md) | Network module — SoftAP / STA / P2P_GO / P2P_CLIENT paths, event handling, WPS |
| [docs/dev-specs/mode-selector.md](docs/dev-specs/mode-selector.md) | Mode selector — `app_wifi_mode` shell command, NVS persistence, factory default |
| [docs/dev-specs/webserver-module.md](docs/dev-specs/webserver-module.md) | Webserver module — HTTP server, REST API endpoints, DNS-SD, web UI |
| [zego/button — button-spec.md](https://github.com/chshzh/zego/blob/main/button/docs/button-spec.md) | Button module — gesture detection (click, double-click, long press), Zbus `BUTTON_CHAN`; provided by **zego/button** |
| [zego/led — led-spec.md](https://github.com/chshzh/zego/blob/main/led/docs/led-spec.md) | LED module — per-LED state machine (static, blink, breathe, marquee), Zbus `LED_CMD_CHAN`; provided by **zego/led** |

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
