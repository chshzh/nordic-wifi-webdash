/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NET_EVENT_MGMT_H
#define NET_EVENT_MGMT_H

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include "../messages.h"

extern const struct zbus_channel CLIENT_CONNECTED_CHAN;

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
 * @brief Wait for the Wi-Fi interface to be up and the WPA supplicant to be ready.
 *
 * Blocks until both NET_EVENT_IF_UP and NET_EVENT_SUPPLICANT_READY have fired.
 * Call this from main() before starting any Wi-Fi mode operation.
 *
 * @param timeout Maximum time to wait (pass K_FOREVER to wait indefinitely)
 * @return 0 on success, negative error code on timeout
 */
int network_wait_for_wpa_supp_ready(k_timeout_t timeout);

/**
 * @brief Wait for a SoftAP client station to connect.
 *
 * @param timeout Maximum time to wait
 * @return 0 on success, -EAGAIN on timeout
 */
int network_wait_for_station_connected(k_timeout_t timeout);

#endif /* NET_EVENT_MGMT_H */
