/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "wifi.h"
#include "../messages.h"
#include "../mode_selector/mode_selector.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_module, CONFIG_WIFI_MODULE_LOG_LEVEL);

#include "net_private.h"
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/dhcpv4_server.h>
#include <zephyr/net/igmp.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/zbus/zbus.h>

/* ============================================================================
 * EXTERNAL CHANNEL (defined in mode_selector.c)
 * ============================================================================
 */

extern const struct zbus_channel WIFI_MODE_CHAN;

/* ============================================================================
 * ZBUS CHANNEL DEFINITION
 * ============================================================================
 */

ZBUS_CHAN_DEFINE(WIFI_CHAN, struct wifi_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));

/* ============================================================================
 * MODULE STATE
 * ============================================================================
 */

static enum wifi_mode active_mode = WIFI_MODE_SOFTAP;

/* SSID captured at L4_CONNECTED; used for disconnect logging and P2P
 * detection.  A SSID starting with "DIRECT-" is a P2P/Wi-Fi Direct session. */
static char sta_ssid[WIFI_SSID_MAX_LEN + 1];

/* Network management event callbacks */
static struct net_mgmt_event_callback wifi_mgmt_cb; /* SoftAP events          */
static struct net_mgmt_event_callback net_mgmt_cb;  /* DHCP bound         */
static struct net_mgmt_event_callback l4_mgmt_cb;   /* L4 connect/disconn */

/* ============================================================================
 * HELPER UTILITIES
 * ============================================================================
 */

static const char *mode_to_str(enum wifi_mode mode)
{
	switch (mode) {
	case WIFI_MODE_SOFTAP:
		return "SoftAP";
	case WIFI_MODE_STA:
		return "STA";
	case WIFI_MODE_P2P:
		return "P2P";
	default:
		return "Unknown";
	}
}

/* ============================================================================
 * SoftAP PATH
 * ============================================================================
 */

static void wifi_softap_start(void)
{
	struct net_if *iface = net_if_get_first_wifi();
	int ret;

	if (!iface) {
		LOG_ERR("No WiFi interface found");
		return;
	}

	/* If conn_mgr queued a STA connection in the race window before
	 * NO_AUTO_CONNECT was set, the SoftAP enable call returns -EBUSY (-16).
	 * Use a direct WPA-level disconnect (NET_REQUEST_WIFI_DISCONNECT)
	 * instead of conn_mgr_all_if_disconnect() to clear any in-progress
	 * attempt WITHOUT firing NET_EVENT_L4_DISCONNECTED.  The conn_mgr
	 * event tears down mDNS multicast group membership on the interface
	 * and it is not reliably restored when the SoftAP comes back up. */
	net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);
	k_sleep(K_MSEC(300));

	/* Set regulatory domain */
	struct wifi_reg_domain regd = {0};

	regd.oper = WIFI_MGMT_SET;
	strncpy(regd.country_code, "US", WIFI_COUNTRY_CODE_LEN + 1);
	ret = net_mgmt(NET_REQUEST_WIFI_REG_DOMAIN, iface, &regd, sizeof(regd));
	if (ret) {
		LOG_WRN("Failed to set regulatory domain: %d (continuing)", ret);
	}

	/* Start DHCP server */
	struct in_addr pool_start;

	ret = inet_pton(AF_INET, "192.168.7.2", &pool_start);
	if (ret == 1) {
		ret = net_dhcpv4_server_start(iface, &pool_start);
		if (ret == -EALREADY) {
			LOG_INF("DHCP server already running");
		} else if (ret < 0) {
			LOG_ERR("Failed to start DHCP server: %d", ret);
		} else {
			LOG_INF("DHCP server started (pool: 192.168.7.2+)");
		}
	}

	/* Enable SoftAP */
	struct wifi_connect_req_params params = {
		.ssid = (uint8_t *)CONFIG_APP_WIFI_SSID,
		.ssid_length = strlen(CONFIG_APP_WIFI_SSID),
		.psk = (uint8_t *)CONFIG_APP_WIFI_PASSWORD,
		.psk_length = strlen(CONFIG_APP_WIFI_PASSWORD),
		.security = WIFI_SECURITY_TYPE_PSK,
		.band = WIFI_FREQ_BAND_2_4_GHZ,
		.channel = 1,
	};

	ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, iface, &params, sizeof(params));
	if (ret) {
		LOG_ERR("Failed to enable SoftAP: %d", ret);
		struct wifi_msg err_msg = {
			.type = WIFI_ERROR,
			.active_mode = WIFI_MODE_SOFTAP,
			.error_code = ret,
		};
		zbus_chan_pub(&WIFI_CHAN, &err_msg, K_NO_WAIT);
		return;
	}

	LOG_INF("SoftAP enable requested: SSID='%s'", CONFIG_APP_WIFI_SSID);
}

/* ============================================================================
 * NETWORK EVENT HANDLERS
 * ============================================================================
 */

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
				    struct net_if *iface)
{
	ARG_UNUSED(iface);

	switch (mgmt_event) {

	case NET_EVENT_WIFI_AP_ENABLE_RESULT: {
		const struct wifi_status *status = (const struct wifi_status *)cb->info;

		if (status->status == 0) {
			struct net_if *ap_iface = net_if_get_first_wifi();
			struct in_addr addr, netmask;

			/* Re-assert the static IP.  The WPA-level disconnect
			 * before AP_ENABLE can remove the manual address. */
			inet_pton(AF_INET, "192.168.7.1", &addr);
			inet_pton(AF_INET, "255.255.255.0", &netmask);
			net_if_ipv4_addr_rm(ap_iface, &addr);
			net_if_ipv4_addr_add(ap_iface, &addr, NET_ADDR_MANUAL, 0);
			net_if_ipv4_set_netmask_by_addr(ap_iface, &addr, &netmask);

			/* Explicitly rejoin the mDNS multicast group (224.0.0.251).
			 * The WPA-level disconnect before AP_ENABLE causes a brief
			 * IF_DOWN which sends an IGMP leave, dropping the membership
			 * that was established during mDNS init.  The mDNS responder's
			 * NET_EVENT_IF_UP rejoin path is guarded by init_listener_done,
			 * which is never set when CONFIG_MDNS_RESPONDER_PROBE=n, so the
			 * framework cannot restore it automatically. */
			struct in_addr mdns_mcast;
			inet_pton(AF_INET, "224.0.0.251", &mdns_mcast);
			int igmp_ret = net_ipv4_igmp_join(ap_iface, &mdns_mcast, NULL);
			if (igmp_ret < 0 && igmp_ret != -EALREADY) {
				LOG_WRN("mDNS IGMP rejoin failed: %d", igmp_ret);
			} else {
				LOG_INF("mDNS 224.0.0.251 (re)joined on SoftAP iface");
			}

			LOG_INF("SoftAP enabled: SSID='%s' IP=192.168.7.1", CONFIG_APP_WIFI_SSID);

			struct wifi_msg msg = {
				.type = WIFI_SOFTAP_STARTED,
				.active_mode = WIFI_MODE_SOFTAP,
				.error_code = 0,
			};
			snprintf(msg.ip_addr, sizeof(msg.ip_addr), "192.168.7.1");
			snprintf(msg.ssid, sizeof(msg.ssid), "%s", CONFIG_APP_WIFI_SSID);
			zbus_chan_pub(&WIFI_CHAN, &msg, K_NO_WAIT);
		} else {
			LOG_ERR("SoftAP enable failed: %d", status->status);

			struct wifi_msg msg = {
				.type = WIFI_ERROR,
				.active_mode = WIFI_MODE_SOFTAP,
				.error_code = status->status,
			};
			zbus_chan_pub(&WIFI_CHAN, &msg, K_NO_WAIT);
		}
		break;
	}

	case NET_EVENT_WIFI_AP_STA_CONNECTED: {
		const struct wifi_ap_sta_info *sta = (const struct wifi_ap_sta_info *)cb->info;
		LOG_INF("SoftAP: client connected %02x:%02x:%02x:%02x:%02x:%02x", sta->mac[0],
			sta->mac[1], sta->mac[2], sta->mac[3], sta->mac[4], sta->mac[5]);
		break;
	}

	case NET_EVENT_WIFI_AP_STA_DISCONNECTED: {
		const struct wifi_ap_sta_info *sta = (const struct wifi_ap_sta_info *)cb->info;
		LOG_INF("SoftAP: client disconnected "
			"%02x:%02x:%02x:%02x:%02x:%02x",
			sta->mac[0], sta->mac[1], sta->mac[2], sta->mac[3], sta->mac[4],
			sta->mac[5]);
		break;
	}

	default:
		break;
	}
}

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
				   struct net_if *iface)
{
	if (mgmt_event != NET_EVENT_IPV4_DHCP_BOUND) {
		return;
	}

	/* Only meaningful in STA or P2P mode */
	if (active_mode != WIFI_MODE_STA && active_mode != WIFI_MODE_P2P) {
		LOG_DBG("DHCP bound in mode %d, ignoring", active_mode);
		return;
	}

	const struct net_if_dhcpv4 *dhcpv4 = cb->info;
	char ip[NET_IPV4_ADDR_LEN] = "0.0.0.0";

	net_addr_ntop(AF_INET, &dhcpv4->requested_ip, ip, sizeof(ip));

	/* Re-query SSID at DHCP time — the iface status is fully settled here,
	 * resolving the race where ssid_len == 0 at L4_CONNECTED time. */
	struct wifi_iface_status wstatus = {0};

	if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &wstatus, sizeof(wstatus)) == 0 &&
	    wstatus.ssid_len > 0) {
		snprintf(sta_ssid, sizeof(sta_ssid), "%.*s", wstatus.ssid_len,
			 (char *)wstatus.ssid);
	}

	/* A SSID starting with "DIRECT-" is a P2P/Wi-Fi Direct connection
	 * regardless of active_mode (P2P shell commands work from STA mode). */
	bool is_p2p = (strncmp(sta_ssid, "DIRECT-", 7) == 0);

	if (is_p2p) {
		LOG_INF("P2P CONNECTED - SSID: %s, IP: %s", sta_ssid, ip);

		struct wifi_msg msg = {
			.type = WIFI_P2P_CONNECTED,
			.active_mode = WIFI_MODE_P2P,
		};
		snprintf(msg.ip_addr, sizeof(msg.ip_addr), "%s", ip);
		snprintf(msg.ssid, sizeof(msg.ssid), "%s", sta_ssid);
		zbus_chan_pub(&WIFI_CHAN, &msg, K_NO_WAIT);
	} else {
		LOG_INF("STA CONNECTED - SSID: %s, IP: %s", sta_ssid[0] ? sta_ssid : "<unknown>",
			ip);

		struct wifi_msg msg = {
			.type = WIFI_STA_CONNECTED,
			.active_mode = WIFI_MODE_STA,
		};
		snprintf(msg.ip_addr, sizeof(msg.ip_addr), "%s", ip);
		snprintf(msg.ssid, sizeof(msg.ssid), "%s", sta_ssid);
		zbus_chan_pub(&WIFI_CHAN, &msg, K_NO_WAIT);
	}
}

static void l4_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
				  struct net_if *iface)
{
	ARG_UNUSED(cb);
	ARG_UNUSED(iface);

	if (active_mode == WIFI_MODE_SOFTAP) {
		return; /* SoftAP uses SoftAP events, not L4 events */
	}

	switch (mgmt_event) {

	case NET_EVENT_L4_CONNECTED: {
		/* Query SSID early — may be empty here if the driver hasn't
		 * settled yet; it is re-queried at DHCP_BOUND time. */
		struct wifi_iface_status status = {0};

		sta_ssid[0] = '\0';
		if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(status)) == 0 &&
		    status.ssid_len > 0) {
			snprintf(sta_ssid, sizeof(sta_ssid), "%.*s", status.ssid_len,
				 (char *)status.ssid);
		}

		bool is_p2p = (strncmp(sta_ssid, "DIRECT-", 7) == 0);

		if (sta_ssid[0]) {
			LOG_INF("WiFi %s connected (SSID: %s)"
				" - awaiting DHCP binding",
				is_p2p ? "P2P" : "STA", sta_ssid);
		} else {
			LOG_INF("WiFi connected - awaiting DHCP binding");
		}
		break;
	}

	case NET_EVENT_L4_DISCONNECTED: {
		/* Detect P2P vs STA from the SSID captured at connect time. */
		bool was_p2p = (strncmp(sta_ssid, "DIRECT-", 7) == 0);

		if (was_p2p || active_mode == WIFI_MODE_P2P) {
			if (sta_ssid[0]) {
				LOG_INF("P2P DISCONNECTED (SSID: %s)", sta_ssid);
			} else {
				LOG_INF("P2P DISCONNECTED");
			}
			struct wifi_msg msg = {
				.type = WIFI_P2P_DISCONNECTED,
				.active_mode = WIFI_MODE_P2P,
			};
			zbus_chan_pub(&WIFI_CHAN, &msg, K_NO_WAIT);
		} else {
			if (sta_ssid[0]) {
				LOG_INF("WiFi STA DISCONNECTED (SSID: %s)", sta_ssid);
			} else {
				LOG_INF("WiFi STA DISCONNECTED");
			}
			LOG_INF("  Reconnect: wifi connect -s <SSID>"
				" -p <password> -k 1");
			struct wifi_msg msg = {
				.type = WIFI_STA_DISCONNECTED,
				.active_mode = WIFI_MODE_STA,
			};
			zbus_chan_pub(&WIFI_CHAN, &msg, K_NO_WAIT);
		}
		sta_ssid[0] = '\0';
		break;
	}

	default:
		break;
	}
}

/* ============================================================================
 * WiFi THREAD
 * ============================================================================
 */

static void wifi_thread_fn(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	LOG_INF("WiFi module thread started (mode: %s)", mode_to_str(active_mode));

	/* Give the network stack a moment to settle before starting Wi-Fi. */
	k_sleep(K_SECONDS(2));

	switch (active_mode) {
	case WIFI_MODE_SOFTAP:
		wifi_softap_start();
		break;

	case WIFI_MODE_STA:
		break;

	case WIFI_MODE_P2P:
		break;

	default:
		LOG_WRN("Unknown Wi-Fi mode %d, falling back to SoftAP", active_mode);
		active_mode = WIFI_MODE_SOFTAP;
		wifi_softap_start();
		break;
	}

	while (1) {
		k_sleep(K_SECONDS(30));
	}
}

K_THREAD_DEFINE(wifi_thread_id, 8192, wifi_thread_fn, NULL, NULL, NULL, 5, 0, 0);

/* ============================================================================
 * MODULE INITIALIZATION (SYS_INIT priority 10)
 * ============================================================================
 */

int wifi_module_init(void)
{
	LOG_INF("Initializing WiFi module");

	/* Read the active mode published by mode_selector (priority 0) */
	struct wifi_mode_msg mode_msg = {.mode = WIFI_MODE_SOFTAP};
	int ret = zbus_chan_read(&WIFI_MODE_CHAN, &mode_msg, K_NO_WAIT);

	if (ret) {
		LOG_WRN("Failed to read WIFI_MODE_CHAN (%d), defaulting to "
			"SoftAP",
			ret);
		active_mode = WIFI_MODE_SOFTAP;
	} else {
		active_mode = mode_msg.mode;
	}

	LOG_INF("Active Wi-Fi mode: %s", mode_to_str(active_mode));

	/* For P2P and SoftAP, block conn_mgr from auto-connecting to any
	 * stored STA credentials.  Must be done at SYS_INIT (priority 10),
	 * before conn_mgr has a chance to kick off a connection attempt. */
	if (active_mode != WIFI_MODE_STA) {
		struct net_if *iface = net_if_get_first_wifi();

		if (iface) {
			conn_mgr_if_set_flag(iface, CONN_MGR_IF_NO_AUTO_CONNECT, true);
			LOG_INF("conn_mgr auto-connect blocked (mode=%s)",
				mode_to_str(active_mode));
		}
	}

	/* SoftAP: SoftAP enable result and client connect/disconnect events */
	net_mgmt_init_event_callback(&wifi_mgmt_cb, wifi_mgmt_event_handler,
				     NET_EVENT_WIFI_AP_ENABLE_RESULT |
					     NET_EVENT_WIFI_AP_STA_CONNECTED |
					     NET_EVENT_WIFI_AP_STA_DISCONNECTED);
	net_mgmt_add_event_callback(&wifi_mgmt_cb);

	/* STA / P2P: DHCP bound → publish connected event with IP and SSID */
	net_mgmt_init_event_callback(&net_mgmt_cb, net_mgmt_event_handler,
				     NET_EVENT_IPV4_DHCP_BOUND);
	net_mgmt_add_event_callback(&net_mgmt_cb);

	/* STA / P2P: L4 connect (early log) and disconnect (notify webserver) */
	net_mgmt_init_event_callback(&l4_mgmt_cb, l4_mgmt_event_handler,
				     NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED);
	net_mgmt_add_event_callback(&l4_mgmt_cb);

	LOG_INF("WiFi module initialized");
	return 0;
}

SYS_INIT(wifi_module_init, APPLICATION, 10);
