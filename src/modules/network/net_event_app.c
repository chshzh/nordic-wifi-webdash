/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * net_event_app.c — webdash-specific network connectivity callbacks.
 *
 * Defines CLIENT_CONNECTED_CHAN and APP_WIFI_STATE_CHAN; overrides __weak
 * hooks from zego/network to publish on both channels.
 *
 * Connectivity paths:
 *   STA / P2P_CLIENT  → zego_on_net_event_dhcp_bound()
 *   SoftAP / P2P_GO   → zego_on_net_event_wifi_ap_enabled() (AP up, no clients)
 *                        zego_on_net_event_wifi_ap_sta_connected() (first client joins)
 *   Disconnect        → zego_on_net_event_wifi_disconnect()
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

ZBUS_CHAN_DEFINE(APP_WIFI_STATE_CHAN, struct app_wifi_state_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.state = APP_WIFI_STATE_CONNECTING, .mode = ZEGO_WIFI_MODE_STA));

/* Cache for SoftAP/P2P_GO AP info — populated in wifi_ap_enabled, used in
 * wifi_ap_sta_connected (which only receives sta_count, not IP/mode/SSID).
 */
static char cached_ap_ip[16];
static char cached_ap_ssid[33];
static enum zego_wifi_mode cached_ap_mode;

/* ── STA / P2P_CLIENT path ─────────────────────────────────────────────── */

void zego_on_net_event_dhcp_bound(enum zego_wifi_mode mode, const char *ip_addr,
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

	struct app_wifi_state_msg state_msg = {
		.state = APP_WIFI_STATE_CONNECTED,
		.mode = mode,
	};

	ret = zbus_chan_pub(&APP_WIFI_STATE_CHAN, &state_msg, K_NO_WAIT);
	if (ret) {
		LOG_WRN("Failed to publish APP_WIFI_STATE_CHAN (CONNECTED): %d", ret);
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

/* ── SoftAP / P2P_GO path ──────────────────────────────────────────────── */

void zego_on_net_event_wifi_ap_enabled(enum zego_wifi_mode mode, const char *ip_addr,
				       const char *ssid)
{
	/* Cache for use in wifi_ap_sta_connected (which only has sta_count). */
	cached_ap_mode = mode;
	snprintf(cached_ap_ip, sizeof(cached_ap_ip), "%s", ip_addr ? ip_addr : "192.168.7.1");
	snprintf(cached_ap_ssid, sizeof(cached_ap_ssid), "%s", ssid ? ssid : "");

	/* AP is up but no client yet — update LED state to SOFTAP (rotate). */
	struct app_wifi_state_msg state_msg = {
		.state = APP_WIFI_STATE_SOFTAP,
		.mode = mode,
	};

	int ret = zbus_chan_pub(&APP_WIFI_STATE_CHAN, &state_msg, K_NO_WAIT);

	if (ret) {
		LOG_WRN("Failed to publish APP_WIFI_STATE_CHAN (SOFTAP): %d", ret);
	}

	LOG_INF("AP enabled: mode=%d ip=%s ssid=%s", (int)mode, cached_ap_ip, cached_ap_ssid);
}

void zego_on_net_event_wifi_ap_sta_connected(int sta_count)
{
	/* First client joined the SoftAP/P2P_GO — start the HTTP server. */
	struct dk_wifi_info_msg msg = {
		.active_mode = (enum app_wifi_mode)cached_ap_mode,
		.error_code = 0,
	};

	snprintf(msg.dk_ip_addr, sizeof(msg.dk_ip_addr), "%s", cached_ap_ip);
	snprintf(msg.ssid, sizeof(msg.ssid), "%s", cached_ap_ssid);

	LOG_INF("CLIENT_CONNECTED_CHAN (AP sta=%d): mode=%d ip=%s", sta_count, (int)cached_ap_mode,
		cached_ap_ip);
	int ret = zbus_chan_pub(&CLIENT_CONNECTED_CHAN, &msg, K_NO_WAIT);

	if (ret) {
		LOG_WRN("Failed to publish CLIENT_CONNECTED_CHAN (AP): %d", ret);
	}

	struct app_wifi_state_msg state_msg = {
		.state = APP_WIFI_STATE_SOFTAP,
		.mode = cached_ap_mode,
	};

	ret = zbus_chan_pub(&APP_WIFI_STATE_CHAN, &state_msg, K_NO_WAIT);
	if (ret) {
		LOG_WRN("Failed to publish APP_WIFI_STATE_CHAN (SOFTAP connected): %d", ret);
	}
}

/* ── Disconnect path ───────────────────────────────────────────────────── */

void zego_on_net_event_wifi_disconnect(void)
{
	struct app_wifi_state_msg state_msg = {
		.state = APP_WIFI_STATE_ERROR,
		.mode = ZEGO_WIFI_MODE_STA,
	};

	int ret = zbus_chan_pub(&APP_WIFI_STATE_CHAN, &state_msg, K_NO_WAIT);

	if (ret) {
		LOG_WRN("Failed to publish APP_WIFI_STATE_CHAN (ERROR): %d", ret);
	}

#if CONFIG_ZEGO_WIFI_BLE_PROV
	struct wifi_msg wmsg = {
		.type = WIFI_STA_DISCONNECTED,
		.rssi = 0,
		.error_code = 0,
	};
	int err = zbus_chan_pub(&WIFI_CHAN, &wmsg, K_MSEC(100));

	if (err) {
		LOG_WRN("Failed to publish WIFI_CHAN (DISCONNECTED): %d", err);
	}
#endif
}
