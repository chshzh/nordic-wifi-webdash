/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Wi-Fi mode selector.
 *
 * Loads the persisted Wi-Fi mode from NVS at SYS_INIT time and publishes
 * it on WIFI_MODE_CHAN so the WiFi module can start in the correct mode.
 *
 * The `wifi_mode [SoftAP|STA|P2P]` shell command can be run at any time to change
 * the mode.  It saves the new mode to NVS and performs a cold reboot so
 * the system comes up cleanly in the selected mode.
 */

#include "mode_selector.h"
#include "../messages.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mode_selector_module, CONFIG_MODE_SELECTOR_LOG_LEVEL);

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>
#include <strings.h>
#include <zephyr/zbus/zbus.h>

/* ============================================================================
 * ZBUS CHANNEL DEFINITION
 * ============================================================================
 */

ZBUS_CHAN_DEFINE(WIFI_MODE_CHAN, struct wifi_mode_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(0));

/* ============================================================================
 * MODULE STATE
 * ============================================================================
 */

static enum wifi_mode selected_mode = WIFI_MODE_SOFTAP;

/* ============================================================================
 * NVS / SETTINGS PERSISTENCE
 * ============================================================================
 */

static int settings_set_cb(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	if (strcmp(key, "wifi_mode") == 0 && len == sizeof(uint8_t)) {
		uint8_t val;
		ssize_t rc = read_cb(cb_arg, &val, sizeof(val));

		if (rc >= 0) {
			selected_mode = (enum wifi_mode)val;
		}
	}
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(mode_selector_settings, "app", NULL, settings_set_cb, NULL, NULL);

static int nvs_save_mode(enum wifi_mode mode)
{
	uint8_t val = (uint8_t)mode;
	int ret = settings_save_one("app/wifi_mode", &val, sizeof(val));

	if (ret) {
		LOG_ERR("Failed to save mode to NVS: %d", ret);
	}
	return ret;
}

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

static void publish_mode(enum wifi_mode mode)
{
	struct wifi_mode_msg msg = {.mode = mode};
	int ret = zbus_chan_pub(&WIFI_MODE_CHAN, &msg, K_NO_WAIT);

	if (ret) {
		LOG_ERR("Failed to publish WIFI_MODE_CHAN: %d", ret);
	}
}

/* ============================================================================
 * SHELL COMMAND: wifi_mode [SoftAP|STA|P2P]
 *
 * Can be run at any time — not just at boot.  Saves the new mode to NVS
 * and performs a cold reboot so the system starts cleanly in that mode.
 * ============================================================================
 */

static int cmd_wifi_mode(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_print(sh,
			    "Current mode: %s\r\n"
			    "Usage: wifi_mode [SoftAP|STA|P2P]\r\n"
			    "  SoftAP  (creates own SoftAP, IP 192.168.7.1)\r\n"
			    "  STA     (connects to existing Wi-Fi)\r\n"
			    "  P2P     (Wi-Fi Direct, build with -DSNIPPET=wifi-p2p)\r\n"
			    "Board reboots automatically after mode change.",
			    mode_to_str(selected_mode));
		return 0;
	}

	const char *arg = argv[1];
	enum wifi_mode new_mode;

	if (strcasecmp(arg, "SoftAP") == 0) {
		new_mode = WIFI_MODE_SOFTAP;
	} else if (strcasecmp(arg, "STA") == 0) {
		new_mode = WIFI_MODE_STA;
	} else if (strcasecmp(arg, "P2P") == 0) {
		new_mode = WIFI_MODE_P2P;
	} else {
		shell_error(sh, "Invalid mode '%s'. Use SoftAP, STA, or P2P.", arg);
		return -EINVAL;
	}

	if (new_mode == selected_mode) {
		shell_print(sh, "Already in %s mode, no change.", mode_to_str(selected_mode));
		return 0;
	}

	shell_print(sh, "Switching to %s mode -- rebooting...", mode_to_str(new_mode));

	int ret = nvs_save_mode(new_mode);

	if (ret == 0) {
		LOG_INF("Mode saved: %s -> rebooting", mode_to_str(new_mode));
	}

	/* Brief delay so shell output flushes before the reboot */
	k_sleep(K_MSEC(200));
	sys_reboot(SYS_REBOOT_COLD);

	return 0;
}

SHELL_CMD_ARG_REGISTER(wifi_mode, NULL, "Set Wi-Fi mode and reboot: wifi_mode [SoftAP|STA|P2P]",
		       cmd_wifi_mode, 1, 1);

/* ============================================================================
 * SYS_INIT FUNCTION (APPLICATION priority 0)
 * ============================================================================
 */

static int mode_selector_init(void)
{
	int ret;

	/* Load persisted mode from NVS */
	ret = settings_subsys_init();
	if (ret) {
		LOG_WRN("settings_subsys_init failed (%d), using SoftAP", ret);
		selected_mode = WIFI_MODE_SOFTAP;
	} else {
		selected_mode = WIFI_MODE_SOFTAP; /* default if key absent */
		settings_load_subtree("app");
		LOG_INF("Stored wifi mode: %s", mode_to_str(selected_mode));
	}

	publish_mode(selected_mode);

	return 0;
}

SYS_INIT(mode_selector_init, APPLICATION, 0);

/* ============================================================================
 * PUBLIC API
 * ============================================================================
 */

enum wifi_mode mode_selector_get_active_mode(void)
{
	return selected_mode;
}
