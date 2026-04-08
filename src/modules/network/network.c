/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * network.c — unified Wi-Fi / network event management module
 *
 * Merges the former wifi/wifi.c (mode logic, event handlers, SoftAP bring-up)
 * and network/network.c (iface-up tracking, station tables) into one file,
 * mirroring the structure of nordic-wifi-audio/src/net/net_event_mgmt.c.
 *
 *   SYS_INIT priority ordering
 *   ───────────────────────────
 *   0  mode_selector_init     — reads NVS, publishes WIFI_MODE_CHAN
 *   5  network_module_init    — registers all event callbacks (this file)
 *
 *   Boot sequence in wifi_thread_fn()
 *   ────────────────────────────────
 *   Step 1 — wait iface_up_sem        (NET_EVENT_IF_UP)
 *   Step 2 — wait wifi_ready_sem      (NET_EVENT_SUPPLICANT_READY)
 *   Step 3 — block conn_mgr if needed
 *   Step 4 — start selected Wi-Fi mode
 */

#include "network.h"
#include "../messages.h"
#include "../mode_selector/mode_selector.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(network_module, CONFIG_NETWORK_MODULE_LOG_LEVEL);

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
#include <zephyr/net/wifi_nm.h>
#include <zephyr/zbus/zbus.h>
#include <supp_events.h>

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
static bool dhcp_server_started; /* guards against double DHCP server start */

/* SSID captured at L4_CONNECTED; used for disconnect logging and P2P
 * detection.  A SSID starting with "DIRECT-" is a P2P/Wi-Fi Direct session. */
static char sta_ssid[WIFI_SSID_MAX_LEN + 1];

/* Station tracking (SoftAP mode) */
#define MAX_SOFTAP_STATIONS 4

struct softap_station {
	bool valid;
	uint8_t mac[6];
};

static struct softap_station connected_stations[MAX_SOFTAP_STATIONS];
static K_MUTEX_DEFINE(station_mutex);

/* ============================================================================
 * SEMAPHORES (match audio net_event_mgmt.c naming)
 * ============================================================================
 */

static K_SEM_DEFINE(iface_up_sem, 0, 1);          /* NET_EVENT_IF_UP            */
static K_SEM_DEFINE(wifi_ready_sem, 0, 1);        /* NET_EVENT_SUPPLICANT_READY */
static K_SEM_DEFINE(station_connected_sem, 0, 1); /* SoftAP: first STA joined   */

/* ============================================================================
 * EVENT CALLBACKS (one per logical layer, registered in network_module_init)
 * ============================================================================
 */

static struct net_mgmt_event_callback iface_event_cb; /* L2: IF UP/DOWN     */
static struct net_mgmt_event_callback wpa_event_cb;   /* L3: supplicant rdy */
static struct net_mgmt_event_callback wifi_mgmt_cb;   /* L2: SoftAP AP evts */
static struct net_mgmt_event_callback net_mgmt_cb;    /* L3: DHCP bound     */
static struct net_mgmt_event_callback l4_mgmt_cb;     /* L4: connect/disc   */

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
 * L3: WPA SUPPLICANT EVENT HANDLER  (NET_EVENT_SUPPLICANT_READY / NOT_READY)
 * ============================================================================
 */

static void l3_wpa_supp_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
				      struct net_if *iface)
{
	ARG_UNUSED(cb);
	ARG_UNUSED(iface);

	switch (mgmt_event) {
	case NET_EVENT_SUPPLICANT_READY:
		LOG_INF("WPA supplicant ready — mode %s", mode_to_str(active_mode));
		k_sem_give(&wifi_ready_sem);
		break;
	case NET_EVENT_SUPPLICANT_NOT_READY:
		LOG_WRN("WPA supplicant not ready");
		break;
	default:
		break;
	}
}

/* ============================================================================
 * L2: INTERFACE EVENT HANDLER  (NET_EVENT_IF_UP / NET_EVENT_IF_DOWN)
 * ============================================================================
 */

static void l2_iface_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
				   struct net_if *iface)
{
	char ifname[IFNAMSIZ + 1] = {0};

	net_if_get_name(iface, ifname, sizeof(ifname) - 1);

	switch (mgmt_event) {
	case NET_EVENT_IF_UP:
		LOG_INF("Network interface %s is up", ifname);
		k_sem_give(&iface_up_sem);
		break;
	case NET_EVENT_IF_DOWN:
		LOG_INF("Network interface %s is down", ifname);
		break;
	default:
		break;
	}
}

/* ============================================================================
 * L2: SOFTAP EVENT HANDLER  (AP enable result, STA join/leave)
 * ============================================================================
 */

static void l2_softap_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
				    struct net_if *iface)
{
	ARG_UNUSED(iface);

	switch (mgmt_event) {

	case NET_EVENT_WIFI_AP_ENABLE_RESULT: {
		const struct wifi_status *status = (const struct wifi_status *)cb->info;

		if (status->status != 0) {
			LOG_ERR("SoftAP enable failed: %d", status->status);
			struct wifi_msg msg = {
				.type = WIFI_ERROR,
				.active_mode = WIFI_MODE_SOFTAP,
				.error_code = status->status,
			};

			zbus_chan_pub(&WIFI_CHAN, &msg, K_NO_WAIT);
			break;
		}

		struct net_if *ap_iface = net_if_get_first_wifi();
		struct in_addr addr, netmask;

		/* Re-assert the static IP.  The WPA-level disconnect before
		 * AP_ENABLE can remove the manually-assigned address. */
		inet_pton(AF_INET, "192.168.7.1", &addr);
		inet_pton(AF_INET, "255.255.255.0", &netmask);
		net_if_ipv4_addr_rm(ap_iface, &addr);
		net_if_ipv4_addr_add(ap_iface, &addr, NET_ADDR_MANUAL, 0);
		net_if_ipv4_set_netmask_by_addr(ap_iface, &addr, &netmask);

		/* Explicitly rejoin mDNS multicast (224.0.0.251).
		 * The WPA-level disconnect before AP_ENABLE sends an IGMP leave.
		 * The mDNS responder's NET_EVENT_IF_UP rejoin path is guarded by
		 * init_listener_done (never set when PROBE=n), so the framework
		 * cannot restore it automatically. */
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
		break;
	}

	case NET_EVENT_WIFI_AP_STA_CONNECTED: {
		const struct wifi_ap_sta_info *sta = (const struct wifi_ap_sta_info *)cb->info;
		char mac_str[18];

		snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x", sta->mac[0],
			 sta->mac[1], sta->mac[2], sta->mac[3], sta->mac[4], sta->mac[5]);
		LOG_INF("SoftAP: client connected %s (link_mode=%d)", mac_str, sta->link_mode);

		k_mutex_lock(&station_mutex, K_FOREVER);
		for (int i = 0; i < MAX_SOFTAP_STATIONS; i++) {
			if (!connected_stations[i].valid) {
				connected_stations[i].valid = true;
				memcpy(connected_stations[i].mac, sta->mac, 6);
				break;
			}
		}
		k_mutex_unlock(&station_mutex);
		k_sem_give(&station_connected_sem);
		break;
	}

	case NET_EVENT_WIFI_AP_STA_DISCONNECTED: {
		const struct wifi_ap_sta_info *sta = (const struct wifi_ap_sta_info *)cb->info;
		char mac_str[18];

		snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x", sta->mac[0],
			 sta->mac[1], sta->mac[2], sta->mac[3], sta->mac[4], sta->mac[5]);
		LOG_INF("SoftAP: client disconnected %s", mac_str);

		k_mutex_lock(&station_mutex, K_FOREVER);
		for (int i = 0; i < MAX_SOFTAP_STATIONS; i++) {
			if (connected_stations[i].valid &&
			    memcmp(connected_stations[i].mac, sta->mac, 6) == 0) {
				memset(&connected_stations[i], 0, sizeof(connected_stations[i]));
				break;
			}
		}
		k_mutex_unlock(&station_mutex);
		break;
	}

	default:
		break;
	}
}

/* ============================================================================
 * L3: DHCP BOUND HANDLER  (STA / P2P — publishes WIFI_CHAN connected event)
 * ============================================================================
 */

static void l3_dhcp_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
				  struct net_if *iface)
{
	if (mgmt_event != NET_EVENT_IPV4_DHCP_BOUND) {
		return;
	}

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

/* ============================================================================
 * L4: CONNECT / DISCONNECT HANDLER  (STA / P2P — early SSID capture)
 * ============================================================================
 */

static void l4_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			     struct net_if *iface)
{
	ARG_UNUSED(cb);
	ARG_UNUSED(iface);

	if (active_mode == WIFI_MODE_SOFTAP) {
		return;
	}

	switch (mgmt_event) {

	case NET_EVENT_L4_CONNECTED: {
		struct wifi_iface_status status = {0};

		sta_ssid[0] = '\0';
		if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(status)) == 0 &&
		    status.ssid_len > 0) {
			snprintf(sta_ssid, sizeof(sta_ssid), "%.*s", status.ssid_len,
				 (char *)status.ssid);
		}

		bool is_p2p = (active_mode == WIFI_MODE_P2P) ||
			      (strncmp(sta_ssid, "DIRECT-", 7) == 0);

		LOG_INF("WiFi %s connected (SSID: %s) — awaiting DHCP", is_p2p ? "P2P" : "STA",
			sta_ssid[0] ? sta_ssid : "<unknown>");
		break;
	}

	case NET_EVENT_L4_DISCONNECTED: {
		bool was_p2p = (strncmp(sta_ssid, "DIRECT-", 7) == 0);

		if (was_p2p || active_mode == WIFI_MODE_P2P) {
			LOG_INF("P2P DISCONNECTED (SSID: %s)",
				sta_ssid[0] ? sta_ssid : "<unknown>");
			struct wifi_msg msg = {
				.type = WIFI_P2P_DISCONNECTED,
				.active_mode = WIFI_MODE_P2P,
			};

			zbus_chan_pub(&WIFI_CHAN, &msg, K_NO_WAIT);
		} else {
			LOG_INF("STA DISCONNECTED (SSID: %s)",
				sta_ssid[0] ? sta_ssid : "<unknown>");
			LOG_INF("  Reconnect: wifi connect -s <SSID> -p <password> -k 1");
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
 * SOFTAP BRING-UP
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

	/* Clear any pending STA connection to avoid -EBUSY on AP_ENABLE */
	net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);
	k_sleep(K_MSEC(300));

	/* Regulatory domain (CONFIG_APP_SOFTAP_REG_DOMAIN, default "US") */
	struct wifi_reg_domain regd = {0};

	regd.oper = WIFI_MGMT_SET;
	strncpy(regd.country_code, CONFIG_APP_SOFTAP_REG_DOMAIN, WIFI_COUNTRY_CODE_LEN + 1);
	ret = net_mgmt(NET_REQUEST_WIFI_REG_DOMAIN, iface, &regd, sizeof(regd));
	if (ret) {
		LOG_WRN("Failed to set reg domain %s: %d (continuing)",
			CONFIG_APP_SOFTAP_REG_DOMAIN, ret);
	} else {
		LOG_INF("Regulatory domain set to %s", CONFIG_APP_SOFTAP_REG_DOMAIN);
	}

	/* DHCP server — guarded against double-start on mode re-entry */
	if (!dhcp_server_started) {
		struct in_addr pool_start;

		ret = inet_pton(AF_INET, "192.168.7.2", &pool_start);
		if (ret == 1) {
			ret = net_dhcpv4_server_start(iface, &pool_start);
			if (ret == -EALREADY) {
				LOG_INF("DHCP server already running");
				dhcp_server_started = true;
			} else if (ret < 0) {
				LOG_ERR("Failed to start DHCP server: %d", ret);
			} else {
				dhcp_server_started = true;
				LOG_INF("DHCP server started (pool: 192.168.7.2+)");
			}
		}
	} else {
		LOG_DBG("DHCP server already started, skipping");
	}

	/* Enable AP — band and channel from Kconfig */
	struct wifi_connect_req_params params = {
		.ssid = (uint8_t *)CONFIG_APP_WIFI_SSID,
		.ssid_length = strlen(CONFIG_APP_WIFI_SSID),
		.psk = (uint8_t *)CONFIG_APP_WIFI_PASSWORD,
		.psk_length = strlen(CONFIG_APP_WIFI_PASSWORD),
		.security = WIFI_SECURITY_TYPE_PSK,
#if defined(CONFIG_APP_SOFTAP_BAND_5_GHZ)
		.band = WIFI_FREQ_BAND_5_GHZ,
#else
		.band = WIFI_FREQ_BAND_2_4_GHZ,
#endif
		.channel = CONFIG_APP_SOFTAP_CHANNEL,
	};

	LOG_INF("SoftAP: band=%s ch=%d SSID='%s'",
		IS_ENABLED(CONFIG_APP_SOFTAP_BAND_5_GHZ) ? "5GHz" : "2.4GHz",
		CONFIG_APP_SOFTAP_CHANNEL, CONFIG_APP_WIFI_SSID);

	ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, iface, &params, sizeof(params));
	if (ret) {
		LOG_ERR("Failed to enable SoftAP: %d", ret);
		struct wifi_msg msg = {
			.type = WIFI_ERROR,
			.active_mode = WIFI_MODE_SOFTAP,
			.error_code = ret,
		};

		zbus_chan_pub(&WIFI_CHAN, &msg, K_NO_WAIT);
	} else {
		LOG_INF("SoftAP enable requested: SSID='%s'", CONFIG_APP_WIFI_SSID);
	}
}

/* ============================================================================
 * WIFI / NETWORK THREAD — sequenced boot matching audio net_event_mgmt.c
 * ============================================================================
 */

static void wifi_thread_fn(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	LOG_INF("Network thread started (mode: %s)", mode_to_str(active_mode));

	/* Step 1 — wait for network interface up (NET_EVENT_IF_UP) */
	k_sem_take(&iface_up_sem, K_FOREVER);
	LOG_INF("Network interface up");

	/* Step 2 — wait for WPA supplicant ready */
	k_sem_take(&wifi_ready_sem, K_FOREVER);
	LOG_INF("WPA supplicant ready");

	/* Step 3 — block conn_mgr auto-connect in non-STA modes */
	if (active_mode != WIFI_MODE_STA) {
		struct net_if *iface = net_if_get_first_wifi();

		if (iface) {
			conn_mgr_if_set_flag(iface, CONN_MGR_IF_NO_AUTO_CONNECT, true);
			LOG_INF("conn_mgr auto-connect blocked (mode=%s)",
				mode_to_str(active_mode));
		}
	}

	/* Step 4 — activate selected mode */
	switch (active_mode) {
	case WIFI_MODE_SOFTAP:
		wifi_softap_start();
		break;
	case WIFI_MODE_STA:
		/* STA: user connects via shell; conn_mgr handles DHCP */
		break;
	case WIFI_MODE_P2P:
		/* P2P: user drives via wifi p2p shell commands */
		break;
	default:
		LOG_WRN("Unknown mode %d, falling back to SoftAP", active_mode);
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
 * PUBLIC API
 * ============================================================================
 */

int network_wait_for_station_connected(k_timeout_t timeout)
{
	return k_sem_take(&station_connected_sem, timeout);
}

/* ============================================================================
 * MODULE INITIALIZATION  (SYS_INIT APPLICATION priority 5)
 * ============================================================================
 */

int network_module_init(void)
{
	LOG_INF("Initializing network module");

	/* Read mode from WIFI_MODE_CHAN (published by mode_selector at priority 0) */
	struct wifi_mode_msg mode_msg = {.mode = WIFI_MODE_SOFTAP};
	int ret = zbus_chan_read(&WIFI_MODE_CHAN, &mode_msg, K_NO_WAIT);

	if (ret) {
		LOG_WRN("Failed to read WIFI_MODE_CHAN (%d), defaulting to SoftAP", ret);
		active_mode = WIFI_MODE_SOFTAP;
	} else {
		active_mode = mode_msg.mode;
	}
	LOG_INF("Active Wi-Fi mode: %s", mode_to_str(active_mode));

	/* Step 1: interface UP/DOWN — register first */
	net_mgmt_init_event_callback(&iface_event_cb, l2_iface_event_handler,
				     NET_EVENT_IF_UP | NET_EVENT_IF_DOWN);
	net_mgmt_add_event_callback(&iface_event_cb);
	/* Boot-time race: NET_EVENT_IF_UP may have fired before this callback
	 * was registered.  Check if the WiFi interface is already up now. */
	{
		struct net_if *wifi_iface = net_if_get_first_wifi();

		if (wifi_iface && net_if_is_up(wifi_iface)) {
			LOG_INF("Wi-Fi interface already up");
			k_sem_give(&iface_up_sem);
		}
	}
	LOG_DBG("Interface event handler registered");

	/* Step 2: WPA supplicant ready — direct net_mgmt (no wifi_ready lib) */
	net_mgmt_init_event_callback(&wpa_event_cb, l3_wpa_supp_event_handler,
				     NET_EVENT_SUPPLICANT_READY | NET_EVENT_SUPPLICANT_NOT_READY);
	net_mgmt_add_event_callback(&wpa_event_cb);
	/* Handle boot-time race: on fast-boot boards NET_EVENT_SUPPLICANT_READY
	 * can fire during SYS_INIT before this callback is registered.  Check if
	 * the supplicant already has an interface and give the semaphore now. */
	{
		struct net_if *wifi_iface = net_if_get_first_wifi();

		if (wifi_iface && wifi_nm_get_instance_iface(wifi_iface) != NULL) {
			LOG_INF("WPA supplicant already ready");
			k_sem_give(&wifi_ready_sem);
		}
	}
	LOG_DBG("WPA supplicant event handler registered");

	/* Step 3: SoftAP AP events */
	net_mgmt_init_event_callback(&wifi_mgmt_cb, l2_softap_event_handler,
				     NET_EVENT_WIFI_AP_ENABLE_RESULT |
					     NET_EVENT_WIFI_AP_STA_CONNECTED |
					     NET_EVENT_WIFI_AP_STA_DISCONNECTED);
	net_mgmt_add_event_callback(&wifi_mgmt_cb);
	LOG_DBG("SoftAP event handler registered");

	/* Step 4: STA/P2P DHCP → publish Zbus connected event */
	net_mgmt_init_event_callback(&net_mgmt_cb, l3_dhcp_event_handler,
				     NET_EVENT_IPV4_DHCP_BOUND);
	net_mgmt_add_event_callback(&net_mgmt_cb);
	LOG_DBG("DHCP event handler registered");

	/* Step 5: L4 connect/disconnect for early SSID capture and disconnect notify */
	net_mgmt_init_event_callback(&l4_mgmt_cb, l4_event_handler,
				     NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED);
	net_mgmt_add_event_callback(&l4_mgmt_cb);
	LOG_DBG("L4 event handler registered");

	LOG_INF("Network module initialized");
	return 0;
}

SYS_INIT(network_module_init, APPLICATION, 5);
