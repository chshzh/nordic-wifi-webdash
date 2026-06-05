/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * net_event_app.c — webdash-specific network connectivity callbacks.
 *
 * Defines CLIENT_CONNECTED_CHAN and overrides the __weak hooks from
 * zego/network/src/net_event_mgmt.c to publish on that channel.
 * When CONFIG_ZEGO_WIFI_BLE_PROV is enabled, also publishes to WIFI_CHAN
 * so the BLE advertisement reflects the current connection state.
 */

#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>
#include <wifi.h>
#include "../messages.h"
#include "net_event_mgmt.h"
#if CONFIG_ZEGO_WIFI_BLE_PROV
#include <wifi_ble_prov.h>
#endif

LOG_MODULE_DECLARE(zego_net_event_mgmt, CONFIG_ZEGO_NETWORK_LOG_LEVEL);

ZBUS_CHAN_DEFINE(CLIENT_CONNECTED_CHAN, struct dk_wifi_info_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(0));

void zego_network_on_wifi_connected(enum zego_wifi_mode mode, const char *ip_addr,
				    const char *mac_addr, const char *ssid)
{
	struct dk_wifi_info_msg msg = {
		.active_mode = (enum app_wifi_mode)mode,
		.error_code = 0,
	};

	snprintf(msg.dk_ip_addr, sizeof(msg.dk_ip_addr), "%s", ip_addr ? ip_addr : "");
	snprintf(msg.dk_mac_addr, sizeof(msg.dk_mac_addr), "%s", mac_addr ? mac_addr : "");
	snprintf(msg.ssid, sizeof(msg.ssid), "%s", ssid ? ssid : "");

	LOG_INF("CLIENT_CONNECTED_CHAN: mode=%d ip=%s ssid=%s", (int)mode, msg.dk_ip_addr,
		msg.ssid);
	int ret = zbus_chan_pub(&CLIENT_CONNECTED_CHAN, &msg, K_NO_WAIT);

	if (ret) {
		LOG_WRN("Failed to publish CLIENT_CONNECTED_CHAN: %d", ret);
	}

#if CONFIG_ZEGO_WIFI_BLE_PROV
	struct wifi_msg wmsg = {
		.type = WIFI_STA_CONNECTED,
		.rssi = 0,
		.error_code = 0,
	};
	int err = zbus_chan_pub(&WIFI_CHAN, &wmsg, K_MSEC(100));

	if (err) {
		LOG_WRN("Failed to publish WIFI_CHAN (CONNECTED): %d", err);
	}
#endif
}

#if CONFIG_ZEGO_WIFI_BLE_PROV
void zego_network_on_wifi_disconnected(void)
{
	struct wifi_msg wmsg = {
		.type = WIFI_STA_DISCONNECTED,
		.rssi = 0,
		.error_code = 0,
	};
	int err = zbus_chan_pub(&WIFI_CHAN, &wmsg, K_MSEC(100));

	if (err) {
		LOG_WRN("Failed to publish WIFI_CHAN (DISCONNECTED): %d", err);
	}
}
#endif
