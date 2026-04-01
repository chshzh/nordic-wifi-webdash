/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file wifi.h
 * @brief Multi-mode WiFi module header (v2.0: SoftAP / STA / P2P)
 */

#ifndef WIFI_H
#define WIFI_H

#include <zephyr/kernel.h>
#include "../messages.h"

/**
 * @brief Initialize WiFi module.
 *
 * Called automatically via SYS_INIT at APPLICATION priority 10.
 * Reads WIFI_MODE_CHAN (published by mode_selector at priority 0) to determine
 * which Wi-Fi path to activate.
 *
 * @return 0 on success, negative error code on failure.
 */
int wifi_module_init(void);

#endif /* WIFI_H */
