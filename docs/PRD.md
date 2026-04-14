# Product Requirements Document — Nordic Wi-Fi WebDash

## Document Information

| Field | Value |
|---|---|
| Product Name | Nordic Wi-Fi WebDash |
| Product ID | nordic-wifi-webdash |
| Version | 2026-04-09-12-00 |
| Previous Version | 2026-03-31 |
| Status | Draft |
| Product Manager | Charlie Shao |
| NCS Version | v3.2.4 |
| Target Board(s) | nRF7002DK, nRF54LM20DK + nRF7002EBII |

---

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-04-14-11-00 | P2P_GO auto-start: firmware automatically runs group_add and sets WPS PIN at boot; waits up to 5 min for first client (same pattern as SoftAP); no manual shell commands needed to start a P2P_GO session |
| 2026-04-14-10-00 | Code sync: P2P split into P2P_GO (device is GO) and P2P_CLIENT (device joins phone group); default mode on fresh flash changed to P2P_GO; app_wifi_mode command updated to [softap|sta|p2p_go|p2p_client]; WPS PIN connection method added; /api/system fields updated to device_ip, device_mac, client_ip, board |
| 2026-04-09-12-00 | Replace Button-1 long-press mode selection with `app_wifi_mode` shell command; remove Wi-Fi credential storage (STA is session-based); remove Cloud & Monitoring placeholder section; P2P now supported on both boards; update target users to Evaluator + Application Developer |
| 2026-04-09-09-00 | Regenerated to PRD template; updated architecture (wifi merged into network module, SYS_INIT priority 5); added DNS-SD `_http._tcp.local` service discovery (FR-104); corrected P2P connection method to `pbc`; removed Kconfig / Flash / RAM / architecture diagrams (→ engineering specs) |
| 2026-03-31 | v2.0.0 — STA + P2P modes, boot-time mode selector, `/api/system` endpoint, NVS persistence |
| 2026-02-02 | v1.0.0 — Initial release — SoftAP only |

---

## 1. Executive Summary

### 1.1 Product Overview

Nordic Wi-Fi WebDash (`nordic-wifi-webdash`) is an IoT demonstration and reference platform for **nRF7x series Wi-Fi development kits**. It serves a real-time, browser-based dashboard directly from the nRF device — no cloud required. Users can view and control device GPIO (buttons and LEDs) from any browser on the same network.

The device supports three Wi-Fi modes: SoftAP (creates its own Wi-Fi hotspot), STA (joins an existing network), and P2P/Wi-Fi Direct (connects directly to a phone without a router). The active mode is selected at boot and persisted in flash so it survives power cycles.

### 1.2 Problem Statement

Developers evaluating nRF7x Wi-Fi need connectivity flexibility:

- **STA mode** is needed for demos where the device must coexist on a company or home network without forcing users to disconnect from their existing Wi-Fi.
- **P2P mode** is needed for field demos where no Wi-Fi router is available at all.
- Earlier designs locked the device into SoftAP mode only, forcing users off their normal network.

### 1.3 Target Users

| User type | Description |
|---|---|
| Evaluator | Sales engineers and FAEs who flash a pre-built `.hex` and run a live demo without modifying code |
| Application Developer | Embedded developers and IoT hobbyists who build from source, customise the firmware, and use this as a reference for their own nRF70 application |

### 1.4 Success Metrics

| Metric | Target | How to measure |
|---|---|---|
| Build success | Clean build for all boards and all modes | `west build` / CI pipeline |
| Dashboard loads in SoftAP | < 2 seconds after connecting to AP | Manual test with browser |
| STA connects from boot | < 30 seconds after power-on | UART log timestamps |
| P2P connects (user-assisted) | < 120 seconds from `wifi p2p find` | Manual test |
| Mode persists across power cycle | Selected mode unchanged after power-off/on | TC-005 |
| Time to first working demo | < 15 minutes from flash | User feedback |

---

## 2. Device Capabilities

### 2.1 Wi-Fi Connectivity

- [x] **Connect to an existing Wi-Fi network (STA mode)** — device joins a network for the current session using `wifi connect -s <SSID> -p <password> -k 1`; dashboard reachable at the DHCP IP or `http://nrfwebdash.local`
- [x] **Create its own Wi-Fi hotspot (SoftAP mode)** — device creates the `WebDash_AP` access point (password `12345678`); dashboard reachable at `http://192.168.7.1`; max 2 client stations
- [x] **Connect directly to a phone without a router (P2P / Wi-Fi Direct)** — two sub-modes:
  - **P2P_GO**: device **automatically** creates the P2P group and activates WPS PIN at boot (no manual shell commands needed); the WPS PIN is logged to the serial console; the device waits up to **5 minutes** for a phone to connect; once connected, phone joins and device assigns IPs; same auto-start pattern as SoftAP
  - **P2P_CLIENT**: device discovers peers and joins the phone's group (`wifi p2p connect <MAC> pbc`); phone assigns IPs
  - Supported on both boards with `-DSNIPPET=wifi-p2p`

The active mode is changed at runtime with `uart:~$ app_wifi_mode [softap|sta|p2p_go|p2p_client]`. The choice is saved to NVS and takes effect after reboot. **Default on fresh flash is P2P_GO.**

### 2.2 Communication & Protocols

- [x] **Web interface** — device serves a live HTML dashboard; button states auto-refresh every 500 ms
- [x] **REST API** — device exposes HTTP endpoints for button state, LED control, and system info
- [x] **Reachable by name** — device advertises `nrfwebdash.local` via mDNS, and registers `_http._tcp.local` via DNS-SD so browsers and zero-conf tools discover it automatically

### 2.3 Storage & Memory

- [x] **Remember Wi-Fi mode after power-off** — selected mode (SoftAP / STA / P2P_GO / P2P_CLIENT) stored in NVS; restored automatically on next boot
- Default mode on **fresh flash** is P2P_GO
- STA connections are session-based; no credentials are stored permanently

### 2.4 Buttons & LEDs

| Hardware | nRF7002DK | nRF54LM20DK + nRF7002EBII |
|---|---|---|
| Buttons available | 2 | 3 (BUTTON 3 unavailable with shield) |
| LEDs available | 2 | 4 |

| Button | Behavior |
|---|---|
| All buttons | Monitored and displayed in real time on the WebDash |

| LED | Meaning |
|---|---|
| All LEDs | User-controllable via web UI or REST API (on / off / toggle) |

### 2.5 Developer & Debug Features

- [x] **Serial shell** — developer can run `app_wifi_mode [softap|sta|p2p_go|p2p_client]` to switch mode, `wifi connect` to join a network, `wifi p2p find/peer/connect` for P2P_CLIENT, `wifi scan` and `wifi status` for diagnostics; in P2P_GO mode the auto-start sequence runs at boot so no manual `wifi p2p group_add` or `wifi wps_pin` is needed
- [x] **Startup log** — board name, MAC address, active Wi-Fi mode, build date, and IP address logged at boot

---

## 3. Functional Requirements

### P0 — Must Have

| ID | As a… | I want to… | So that… | Acceptance Criteria | Engineering Spec |
|---|---|---|---|---|---|
| FR-001 | user | power on in SoftAP mode and open the dashboard | I can demo the device with no network infrastructure | - `WebDash_AP` SSID visible<br>- Dashboard loads at `http://192.168.7.1`<br>- Max 2 client stations enforced | [network-module.md](specs/network-module.md) |
| FR-002 | user | connect the device to an existing Wi-Fi network (STA) and access the dashboard | I can demo while staying on my office network | - Run `wifi connect -s <SSID> -p <password> -k 1` in shell<br>- Dashboard at DHCP IP or `http://nrfwebdash.local`<br>- `[network] STA CONNECTED IP: <x>` logged | [network-module.md](specs/network-module.md) |
| FR-003 | user | connect my phone directly to the device via Wi-Fi Direct (P2P) and access the dashboard | I can demo where no Wi-Fi router is available | - **P2P_GO**: device auto-creates the P2P group and activates WPS PIN at boot; WPS PIN logged to serial console; device waits up to 5 min for first client; phone joins via Wi-Fi Direct → PIN method; dashboard at P2P IP after client connects<br>- **P2P_CLIENT**: auto P2P find at boot; `wifi p2p connect <MAC> pbc` connects to phone’s group<br>- Supported on both boards | [network-module.md](specs/network-module.md) |
| FR-004 | user | switch the Wi-Fi mode with a shell command and have it persist | I can change modes on the fly without reflashing | - `uart:~$ app_wifi_mode [SoftAP\|STA\|P2P_GO\|P2P_CLIENT]` saves mode to NVS<br>- Mode takes effect after reboot<br>- Factory default (fresh flash): P2P_GO | [mode-selector.md](specs/mode-selector.md) |
| FR-005 | user | have the selected Wi-Fi mode remembered after power-off | I don’t have to reconfigure after every power cycle | - Mode stored in NVS<br>- Factory default (fresh flash): P2P_GO<br>- No button press needed on subsequent boots | [mode-selector.md](specs/mode-selector.md) |
| FR-006 | user | see button states and control LEDs in the browser | I can verify GPIO is working during demos | - Buttons show correct count per board (2 or 3)<br>- LEDs show correct count per board (2 or 4)<br>- ON / OFF / Toggle responds in < 100 ms | [webserver-module.md](specs/webserver-module.md) |
| FR-007 | user | see the active Wi-Fi mode and device IP address in the dashboard | I know at a glance which mode is active | - Mode banner shows SoftAP / STA / P2P_GO / P2P_CLIENT<br>- Current IP address displayed<br>- `/api/system` returns `{mode, device_ip, device_mac, client_ip, ssid, uptime_s, board}` | [webserver-module.md](specs/webserver-module.md) |

### P1 — Should Have

| ID | As a… | I want to… | So that… | Acceptance Criteria | Engineering Spec |
|---|---|---|---|---|---|
| FR-101 | developer | call a REST API to read device state | I can integrate the device into custom tooling | - `GET /api/buttons` → JSON<br>- `GET /api/leds` → JSON<br>- `POST /api/led` → LED control<br>- `GET /api/system` → mode + IP | [webserver-module.md](specs/webserver-module.md) |
| FR-102 | developer | use shell commands over UART for Wi-Fi diagnostics and mode changes | I can inspect and test connectivity without a browser | - `app_wifi_mode [SoftAP\|STA\|P2P_GO\|P2P_CLIENT]` switches and persists mode<br>- `wifi connect -s <SSID> -p <pwd> -k 1` joins a network (STA)<br>- `wifi scan`, `wifi status` work<br>- P2P_CLIENT: `wifi p2p find / peer / connect` work<br>- P2P_GO: group and WPS PIN auto-start at boot; no manual shell commands required | [network-module.md](specs/network-module.md) |
| FR-103 | developer | see board name, MAC, mode, and IP in the startup log | I can confirm firmware state without opening a browser | - Board ID, MAC, build date logged at boot<br>- Active mode logged<br>- IP logged when connected | [architecture.md](specs/architecture.md) |
| FR-104 | user | discover the device automatically without knowing its IP | I can open the dashboard just by name | - Device registers `_http._tcp.local` via DNS-SD<br>- Device reachable at `http://nrfwebdash.local` via mDNS | [webserver-module.md](specs/webserver-module.md) |

### P2 — Nice to Have

| ID | As a… | I want to… | So that… | Acceptance Criteria | Engineering Spec |
|---|---|---|---|---|---|
| FR-201 | user | customise SoftAP credentials without rebuilding | I can use my own SSID/password on the hotspot | - `overlay-wifi-credentials.conf` supported<br>- Template file tracked in git, actual overlay gitignored | [network-module.md](specs/network-module.md) |
| FR-202 | developer | see heap usage logged periodically | I can detect memory growth during long-running tests | - Heap high-water mark logged every N minutes<br>- Warning at configurable threshold | [architecture.md](specs/architecture.md) |

---

## 4. Non-Functional Requirements

### 4.1 Performance

| Behaviour | Target |
|---|---|
| Dashboard page load | < 2 seconds |
| Button state auto-refresh interval | 500 ms |
| LED control response (web → device) | < 100 ms |
| SoftAP client connection | < 10 seconds |
| STA connection (after `wifi connect` command) | < 30 seconds |
| P2P connection (user-assisted, P2P_CLIENT) | < 120 seconds from device boot |
| P2P_GO ready for client | < 10 seconds from boot (auto group_add + WPS PIN) |
| P2P_GO client connect timeout | 5 minutes; group stays alive after timeout |

### 4.2 Reliability

| Expectation | Target |
|---|---|
| Continuous operation without restart | 24 hours |
| Auto-reconnect in STA mode after AP power cycle | Yes — no user action needed |
| Auto P2P re-discovery after group removal | Yes |
| Boot into last saved mode | Yes — no button press required |

### 4.3 Security

| Expectation | Requirement |
|---|---|
| Wi-Fi credentials (STA) | Session-based; entered in the shell at runtime; never stored in NVS or committed to source control |
| SoftAP credentials | Configurable via overlay file; template tracked in git, actual value gitignored |

---

## 5. Hardware

### 5.1 Target Development Kits

| Board | Wi-Fi chip | Buttons | LEDs | Supported modes |
|---|---|---|---|---|
| nRF7002DK | nRF7002 (built in) | 2 | 2 | SoftAP, STA, P2P |
| nRF54LM20DK + nRF7002EBII shield | nRF7002 (shield) | 3 | 4 | SoftAP, STA, P2P |

*Note: BUTTON 3 on nRF54LM20DK is unavailable when the nRF7002EBII shield is attached.*

### 5.2 Board-specific notes

- Both boards support SoftAP, STA, and P2P modes. Build with `-DSNIPPET=wifi-p2p` to enable P2P.
- BUTTON 3 on nRF54LM20DK is unavailable when the nRF7002EBII shield is attached; BUTTON 0–2 remain usable.

---

## 6. User Experience

### 6.1 First-time Setup (Evaluator)

1. Download the pre-built `.hex` from the [Releases](https://github.com/chshzh/nordic-wifi-webdash/releases) page.
2. Flash with **nRF Connect for Desktop → Programmer → Erase & Write**.
3. Open a serial terminal at 115200 baud.
4. The device starts in SoftAP mode automatically.
5. Connect to Wi-Fi `WebDash_AP` (password `12345678`), then open `http://192.168.7.1` — the dashboard appears.

**To switch to STA mode:**

1. In the serial terminal: `uart:~$ app_wifi_mode STA` — mode is saved and device reboots.
2. After reboot: `uart:~$ wifi connect -s <SSID> -p <password> -k 1`
3. Open `http://<DHCP-IP>` or `http://nrfwebdash.local`.

**To switch to P2P mode:**

1. In the serial terminal: `uart:~$ app_wifi_mode P2P` — mode is saved and device reboots.
2. Follow the Wi-Fi Direct instructions printed in the terminal.

### 6.2 Normal Operation

- Device boots automatically into the last saved Wi-Fi mode.
- Dashboard auto-refreshes button and LED states every 500 ms.
- In STA mode: reachable at `http://nrfwebdash.local` (or the DHCP IP shown in the UART log).
- In SoftAP mode: connect to `WebDash_AP` (password `12345678`), then open `http://192.168.7.1`.

### 6.3 Mode Switching

Switch mode at any time from the serial shell:

```
uart:~$ app_wifi_mode SoftAP
uart:~$ app_wifi_mode STA
uart:~$ app_wifi_mode P2P
```

The selected mode is saved to NVS and takes effect after the device reboots. There is no need to hold buttons or use a boot-time menu.

**STA connection sequence** (once booted in STA mode):

1. Device logs: `[network] STA mode active.`
2. Run `wifi connect -s <SSID> -p <password> -k 1` to join the network.
3. Open the dashboard at the IP shown in the serial log or at `http://nrfwebdash.local`.

**P2P connection sequence** (once booted in P2P mode):

1. Device logs: `[network] P2P mode active. Auto-starting peer discovery...`
2. Run `wifi p2p peer` to list discovered phones.
3. Run `wifi p2p connect <MAC> pbc -g 0` to connect.
4. Accept the connection request on the phone.
5. Open the dashboard at the P2P IP shown in the serial log (e.g. `http://192.168.49.x`).

### 6.4 Troubleshooting

| Symptom | What to do |
|---|---|
| `WebDash_AP` not visible | Wait 30 s; verify the board is flashed; check serial log |
| Browser shows "can't connect to site" | Confirm your laptop is connected to `WebDash_AP`, not its normal Wi-Fi |
| STA mode: device not reachable by name | Try the IP address from the UART log; `nrfwebdash.local` requires mDNS support in your OS |
| P2P: phone doesn't appear in `wifi p2p peer` | Run `wifi p2p find` again; move phone closer; ensure phone Wi-Fi Direct is enabled |

---

## 7. Release Criteria

All P0 requirements (FR-001 to FR-007) must pass on all supported boards before release.

- [ ] FR-001 pass: SoftAP dashboard accessible at `http://192.168.7.1`
- [ ] FR-002 pass: STA dashboard accessible after `wifi connect` command
- [ ] FR-003 pass: P2P dashboard accessible from phone (both boards)
- [ ] FR-004 pass: `app_wifi_mode` command saves mode; device reboots into new mode
- [ ] FR-005 pass: Selected mode survives power-off/on without re-entering command
- [ ] FR-006 pass: Buttons and LEDs visible and controllable in browser
- [ ] FR-007 pass: Mode banner and IP address shown in dashboard
- [ ] Clean `west build` for all boards — no errors, no warnings
- [ ] Device runs 24 hours without restart (SoftAP soak test)
- [ ] No credentials printed in UART logs or committed to source control
- [ ] README Quick Start guide tested by a Evaluator (non-developer)

---

## 8. Open Questions

| # | Question | Owner | Due |
|---|---|---|---|
| 1 | Should the web UI display a signal quality indicator (RSSI) in STA mode? | PM | TBD |
| 2 | Should Button 2 have a dedicated function (e.g. trigger reconnect in STA mode)? | PM | TBD |

---

## 9. Engineering Spec References

| Spec file | Covers |
|---|---|
| [architecture.md](specs/architecture.md) | System design, Zbus channels, module map, boot sequence, SYS_INIT priorities |
| [network-module.md](specs/network-module.md) | Unified network module: SoftAP / STA / P2P paths, net event handling |
| [mode-selector.md](specs/mode-selector.md) | Boot window, NVS persistence, shell menu |
| [button-module.md](specs/button-module.md) | GPIO button monitoring, press/release events, board differences |
| [webserver-module.md](specs/webserver-module.md) | REST API, DNS-SD `_http._tcp.local`, mode banner, web UI |

---

*Document ID: PRD-nordic-wifi-webdash-2026-04-09 | Classification: Public / Open Source*

