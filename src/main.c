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
	case APP_WIFI_MODE_SOFTAP:
		mode_str = "SoftAP";
		break;
	case APP_WIFI_MODE_STA:
		mode_str = "STA";
		break;
	case APP_WIFI_MODE_P2P_GO:
		mode_str = "P2P_GO";
		break;
	case APP_WIFI_MODE_P2P_CLIENT:
		mode_str = "P2P_CLIENT";
		break;
	default:
		mode_str = "Unknown";
		break;
	}

	LOG_INF("==============================================");
	LOG_INF("Nordic Wi-Fi WebDash");
	LOG_INF("==============================================");
	LOG_INF("Build: %s %s", __DATE__, __TIME__);
	LOG_INF("Board: %s", board_name);

	if (mac_addr && mac_addr->len == 6) {
		LOG_INF("MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac_addr->addr[0], mac_addr->addr[1],
			mac_addr->addr[2], mac_addr->addr[3], mac_addr->addr[4], mac_addr->addr[5]);
	}

	LOG_INF("Current active Wi-Fi mode: %s", mode_str);

	LOG_INF("Type 'wifi_mode [SoftAP|STA|P2P_GO|P2P_CLIENT]' to change mode after reboot.");
	LOG_INF("==============================================");
	LOG_INF("WebDash connection instructions:");

	switch (mode_selector_get_active_mode()) {
	case APP_WIFI_MODE_SOFTAP:
		LOG_INF("Connect AP SSID='%s' using Password='%s'", CONFIG_APP_WIFI_SSID,
			CONFIG_APP_WIFI_PASSWORD);
		break;

	case APP_WIFI_MODE_STA:
		LOG_INF("STA mode: connect via shell:");
		LOG_INF("  wifi connect -s <SSID> -p <password> -k 1 -- WPA2");
		LOG_INF("  wifi connect --help                       -- help for more options");
		break;

	case APP_WIFI_MODE_P2P_GO:
		LOG_INF("P2P_GO mode: P2P group + WPS PIN auto-started at boot.");
		LOG_INF("1. Phone: Turn on Wi-Fi, disconnect from other APs");
		LOG_INF("2. Phone: Wi-Fi Direct -> wait for DK, select it, enter PIN 12345678");
			CONFIG_NET_CONFIG_MY_IPV4_ADDR);
			break;

	case APP_WIFI_MODE_P2P_CLIENT:
		LOG_INF("P2P_CLIENT mode: DK joins phone's P2P group:");
		LOG_INF("1. DK:    wifi p2p find              -- search for peers");
		LOG_INF("2. Phone: Enable Wi-Fi Direct, wait for DK MAC to appear");
		LOG_INF("3. DK:    wifi p2p peer              -- list peers, find phone MAC");
		LOG_INF("4. DK:    wifi p2p connect <phone MAC> pbc -g 0  -- connect");
		LOG_INF("5. Phone: Press ACCEPT on the Wi-Fi Direct invitation");
		break;
	}

	LOG_INF("==============================================");

	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}
