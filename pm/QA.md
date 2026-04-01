# NCS Project Quality Assurance Report

**Project Name**: Nordic WiFi SoftAP Webserver
**Project Repository**: /opt/nordic/ncs/myApps/nordic_wifi_softap_webserver
**Review Date**: February 3, 2026
**Reviewer(s)**: GitHub Copilot (Product Manager skill)
**NCS Version**: v3.2.4
**Project Version**: v1.0.1 (per README roadmap)
**Board/Platform**: nrf7002dk/nrf5340/cpuapp (primary), nrf54lm20dk/nrf54lm20a+shield (secondary)

---

## Executive Summary

**Overall Score**: 88 / 100

**Status**: ☑️ **PASS WITH ISSUES**

**Key Findings**:
- Static IP expectations in PRD and README (192.168.1.1) diverge from the shipped configuration (192.168.7.1), which blocks the documented Quick Start flow ([PRD.md](PRD.md#L250-L279), [README.md](README.md#L109-L134), [prj.conf](prj.conf#L35-L50)).
- Wi-Fi credentials are hardcoded and logged verbatim at boot in [src/main.c](src/main.c#L55-L67), violating security guidance and exposing passwords over UART.
- PRD mandates four DK buttons/LEDs, but the shipping build gates down to two buttons on the nRF7002DK without updating the acceptance criteria or marketing copy ([PRD.md](PRD.md#L250-L320), [src/modules/messages.h](src/modules/messages.h#L18-L56)).

**Recommendation**: Release after addressing documentation drift and password handling. Firmware functionality aligns with the PRD, but the discrepancies above must be fixed before another public drop.

---

## Score Breakdown

| Category | Score | Weight | Weighted Score |
|----------|-------|--------|----------------|
| 1. Project Structure | 15/15 | 15% | 15 |
| 2. Core Files Quality | 16/20 | 20% | 16 |
| 3. Configuration | 13/15 | 15% | 13 |
| 4. Code Quality | 17/20 | 20% | 17 |
| 5. Documentation | 11/15 | 15% | 11 |
| 6. Wi-Fi Implementation | 8/10 | 10% | 8 |
| 7. Security | 4/10 | 10% | 4 |
| 8. Build & Testing | 4/10 | 10% | 4 |
| **Total** |  | **100%** | **88** |

---

## 1. Project Structure Review (15/15)
All mandatory artifacts (CMakeLists, Kconfig, prj.conf, LICENSE, README, src/main.c) plus optional workspace resources (west.yml, boards/, sysbuild.conf, www/) are present and well organized. No build artifacts are tracked.

**Issues**: None.

---

## 2. Core Files Quality (16/20)
- **CMakeLists.txt**: Meets all Zephyr requirements, pulls in HTTP resources, and keeps modules modular (5/5).
- **Kconfig**: Clean menu with helpful descriptions, but lacks warnings about credential safety (4/5).
- **prj.conf**: Organized but duplicates `CONFIG_NET_CONFIG_MY_IPV4_*` entries while silently switching the subnet to `192.168.7.1`, contradicting requirements (3/5).
- **src/main.c**: Simple coordinator but logs SSID/password and hardcodes the incorrect IP (4/5).

---

## 3. Configuration Review (13/15)
SoftAP, DHCP server, IPv4 stack, SMF, Zbus, JSON, and logging are properly enabled with sufficient heap (120 KB). Gaps:
- Static IP values do not match PRD/README, breaking FR-001 acceptance testing ([PRD.md](PRD.md#L270-L279), [README.md](README.md#L109-L134), [prj.conf](prj.conf#L35-L50)).
- No configuration guard ensures the four-button requirement is met on every board SKU.

---

## 4. Code Quality Analysis (17/20)
Implementation follows Zephyr style, uses SMF + Zbus effectively, and validates API inputs. Remaining items include adding retry logic to the Wi-Fi SM error state and avoiding password logging.

---

## 5. Documentation Assessment (11/15)
README is comprehensive but inaccurate in key areas: Quick Start instructions and Wi-Fi configuration list `192.168.1.1`, conflicting with the firmware configuration, and REST API examples show four buttons even on boards that only expose two ([README.md](README.md#L109-L219)). PRD remains thorough.

---

## 6. Wi-Fi Implementation (8/10)
SoftAP bring-up, DHCP server, and station tracking operate as expected. However, there is no automatic retry when `NET_REQUEST_WIFI_AP_ENABLE` fails and no instrumentation for the “10 client” success metric ([src/modules/wifi/wifi.c](src/modules/wifi/wifi.c#L94-L189)).

---

## 7. Security Audit (4/10)
- Password is logged in plaintext at boot ([src/main.c](src/main.c#L55-L67)).
- Credentials live in version-controlled Kconfig defaults ([Kconfig](Kconfig#L10-L23)).
- No runtime credential rotation (FR-201) or TLS support.

Immediate remediation: move credentials into an ignored overlay and remove boot-time password logging.

---

## 8. Build & Testing (4/10)
- `check_project.sh` exits prematurely after the first check because of `set -e`, so automated coverage is incomplete.
- No fresh `west build` or on-device validation logs were captured during this review; results rely on historical data referenced in README/PRD.

---

## Issues Summary

### Critical Issues
| # | Category | Description | Impact | Recommendation |
|---|----------|-------------|--------|----------------|
| 1 | Configuration & Docs | Firmware uses 192.168.7.1 but PRD/README/F.R.-001 test flow require 192.168.1.1 ([PRD.md](PRD.md#L270-L279), [README.md](README.md#L109-L134), [prj.conf](prj.conf#L35-L50)). | High | Align defaults and documentation by exposing the subnet via Kconfig and updating README/PRD, or change the firmware back to 192.168.1.1. |
| 2 | Security | SSID/password are committed defaults and logged in plaintext ([Kconfig](Kconfig#L10-L23), [src/main.c](src/main.c#L55-L67)). | High | Stop logging credentials, store production secrets in ignored overlays, and require developers to supply their own passwords. |

### Warnings
| # | Category | Description | Impact | Recommendation |
|---|----------|-------------|--------|----------------|
| 1 | PRD Compliance | Hardware section promises four buttons/LEDs, but nrf7002dk builds only expose two buttons ([PRD.md](PRD.md#L250-L320), [src/modules/messages.h](src/modules/messages.h#L18-L56)). | Medium | Update PRD and README tables to describe per-board capabilities and adjust FR-002 acceptance tests.
| 2 | Documentation | REST payload samples and troubleshooting tips assume 192.168.1.x and four-button JSON responses ([README.md](README.md#L190-L219)). | Medium | Regenerate samples from the active configuration to avoid misleading users.
| 3 | Automation | `check_project.sh` halts after the first PASS, preventing automated validation. | Medium | Remove `set -e` or guard echo commands so the script can finish.

### Improvements
| # | Category | Description | Benefit | Priority |
|---|----------|-------------|---------|----------|
| 1 | Wi-Fi Resilience | Add retry/back-off when SoftAP enable fails ([src/modules/wifi/wifi.c](src/modules/wifi/wifi.c#L123-L189)). | Improves robustness in noisy RF environments | Medium |
| 2 | Telemetry | Instrument concurrent-client counts to prove the 10-client success metric. | Objective data for PRD success metrics | Low |
| 3 | Credential UX | Implement FR-201 (configurable Wi-Fi credentials using settings/NVS). | Better aligns with “Nice to Have” roadmap | Low |

---

## PRD Compliance Snapshot
- **FR-001 and FR-002**: Partially met; incorrect IP and limited button count prevent literal acceptance.
- **FR-003–FR-005**: Implemented via SMF modules and HTTP server.
- **FR-101–FR-103**: REST endpoints and logging exist, but documentation needs refreshed samples.
- **Hardware Requirements**: LEDs meet the spec; button count does not on the primary board.

---

## Recommendations for Project Team

### Immediate (before next release)
1. Synchronize static IP settings across Kconfig/prj.conf/README/PRD and stop hardcoding 192.168.1.1 in docs until the firmware matches.
2. Remove password logging and move credentials into a `.gitignore`d overlay; document the workflow so developers supply their own secrets.
3. Update PRD and README acceptance criteria to describe per-board button/LED capabilities.

### Short-term
1. Add retry/back-off to the Wi-Fi SM error path.
2. Capture fresh build/log evidence (multi-client, latency, uptime) and attach to this report.
3. Fix `check_project.sh` so future reviews regain automated coverage.

### Long-term
1. Implement credential persistence (FR-201) and optional HTTPS per the roadmap.
2. Generate README networking sections directly from Kconfig/prj.conf values to prevent drift.
3. Add instrumentation for success metrics (latency, concurrency, memory) so CI can enforce them.

---

## Recommendations for ncs-project-generate Skill
1. Add checklist items that compare README networking instructions against `CONFIG_NET_CONFIG_MY_IPV4_ADDR` to catch mismatches early.
2. Include guidance/snippets that prevent logging secrets and encourage the use of ignored overlays for credentials.
3. Extend templates to describe per-board hardware capabilities so PRDs automatically adjust acceptance criteria.

---

## Follow-up
- Share this QA.md with the project owner and block release until the two critical issues are addressed.
- After fixes land, rerun the automated script (once patched) and attach fresh build/test evidence.
- Target a re-review ahead of the next roadmap milestone (v1.1).

**Risk Level**: 🟡 Medium – functionality works, but documentation/security gaps risk customer confusion and credential leakage.

**Estimated Effort to Address Issues**: Critical (4 hours), Warnings (6 hours), Improvements (8 hours).

**Next Review**: After documentation alignment and credential handling fixes are completed.
