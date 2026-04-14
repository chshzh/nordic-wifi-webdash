/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file mode_selector.h
 * @brief Wi-Fi mode selector — public API
 *
 * SYS_INIT flow (APPLICATION priority 0):
 *   1. Load persisted mode from NVS (default: SoftAP).
 *   2. Publish the mode on WIFI_MODE_CHAN so the Wi-Fi module starts
 *      in the correct mode.
 *
 * To change mode at any time, run the `app_wifi_mode [SoftAP|STA|P2P]` shell command.
 * It saves the new mode to NVS and performs a cold reboot.
 */

#ifndef MODE_SELECTOR_H
#define MODE_SELECTOR_H

#include "../messages.h"

/**
 * @brief Return the Wi-Fi mode loaded from NVS at boot.
 */
enum app_wifi_mode mode_selector_get_active_mode(void);

#endif /* MODE_SELECTOR_H */
