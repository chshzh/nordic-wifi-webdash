/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <zephyr/kernel.h>
#include "../messages.h"

/**
 * @brief Initialize unified network/Wi-Fi event management module.
 *
 * Called automatically via SYS_INIT at APPLICATION priority 5.
 * Reads WIFI_MODE_CHAN (published by mode_selector at priority 0) to determine
 * which Wi-Fi mode to activate, registers all event callbacks, and starts
 * the Wi-Fi thread.
 *
 * @return 0 on success, negative error code on failure.
 */
int network_module_init(void);

/**
 * @brief Wait for a SoftAP client station to connect.
 *
 * @param timeout Maximum time to wait
 * @return 0 on success, -EAGAIN on timeout
 */
int network_wait_for_station_connected(k_timeout_t timeout);

#endif /* NETWORK_H */
