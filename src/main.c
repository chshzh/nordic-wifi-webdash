/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>

#include "modules/messages.h"
#include "modules/mode_selector/mode_selector.h"
#include "modules/network/net_event_mgmt.h"
#include "modules/network/wifi_utils.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);
/* ============================================================================
 * MAIN APPLICATION
 * ============================================================================
 */

int main(void)
{
	struct net_if *iface = net_if_get_default();
	struct net_linkaddr *mac_addr = net_if_get_link_addr(iface);
	const char *board_name;

	if (strstr(CONFIG_BOARD, "nrf7002dk") != NULL) {
		board_name = "nRF7002DK";
	} else if (strstr(CONFIG_BOARD, "nrf54lm20dk") != NULL) {
		board_name = "nRF54LM20DK+nRF7002EBII";
	} else {
		board_name = CONFIG_BOARD;
	}

	const char *mode_str;

	switch (mode_selector_get_active_mode()) {
	case WIFI_MODE_SOFTAP:
		mode_str = "SoftAP";
		break;
	case WIFI_MODE_STA:
		mode_str = "STA";
		break;
	case WIFI_MODE_P2P:
		mode_str = "P2P";
		break;
	default:
		mode_str = "Unknown";
		break;
	}

	LOG_INF("==============================================");
	LOG_INF("Nordic WiFi Web Dashboard v2.0");
	LOG_INF("==============================================");
	LOG_INF("Build: %s %s", __DATE__, __TIME__);
	LOG_INF("Board: %s", board_name);

	if (mac_addr && mac_addr->len == 6) {
		LOG_INF("MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac_addr->addr[0], mac_addr->addr[1],
			mac_addr->addr[2], mac_addr->addr[3], mac_addr->addr[4], mac_addr->addr[5]);
	}

	LOG_INF("Current active Wi-Fi mode: %s", mode_str);

	LOG_INF("Type 'wifi_mode [SoftAP|STA|P2P]' to change mode after reboot.");
	LOG_INF("==============================================");
	LOG_INF("=============Connect Web Dashboard============");

	switch (mode_selector_get_active_mode()) {
	case WIFI_MODE_SOFTAP:
		LOG_INF("SoftAP mode: SSID='%s' Password='12345678'", CONFIG_APP_WIFI_SSID);
		LOG_INF("Connect and open http://192.168.7.1:%d", CONFIG_APP_HTTP_PORT);
		break;

	case WIFI_MODE_STA:
		LOG_INF("STA mode: connect via shell:");
		LOG_INF("  wifi connect -s <SSID> -p <password> -k 1 -- WPA2");
		LOG_INF("  wifi connect --help                       -- help for more options");
		break;

	case WIFI_MODE_P2P:
		LOG_INF("P2P mode: connect phone peer via shell:");
		LOG_INF("1.DK: wifi p2p find                   -- search for peers");
		LOG_INF("2.Phone: Enable Wi-Fi Direct, then wait device MAC appears.");
		LOG_INF("3.DK: wifi p2p peer                   -- list peers and find phone "
			"MAC");
		LOG_INF("4.DK: wifi p2p connect <phone MAC> pbc -g 0   -- connect to target "
			"phone");
		LOG_INF("5.Phone: Press ACCEPT button on your phone for Invitation to "
			"connect.");
		break;
	}

	LOG_INF("==============================================");

	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}
