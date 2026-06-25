/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file messages.h
 * @brief Common message definitions for all modules
 */

#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

/* ==========================================================================
 * BOARD-SPECIFIC CAPABILITIES
 * ==========================================================================
 */

#if defined(CONFIG_BOARD_NRF7002DK_NRF5340_CPUAPP)
#define APP_NUM_BUTTONS 2
#define APP_NUM_LEDS    2
#define APP_BUTTON_LABELS                                                                          \
	{                                                                                          \
		"Button 1", "Button 2"                                                             \
	}
#define APP_LED_LABELS                                                                             \
	{                                                                                          \
		"LED1", "LED2"                                                                     \
	}
#elif defined(CONFIG_BOARD_NRF54LM20DK_NRF54LM20A_CPUAPP)
#define APP_NUM_BUTTONS 3
#define APP_NUM_LEDS    4
#define APP_BUTTON_LABELS                                                                          \
	{                                                                                          \
		"BUTTON0", "BUTTON1", "BUTTON2"                                                    \
	}
#define APP_LED_LABELS                                                                             \
	{                                                                                          \
		"LED0", "LED1", "LED2", "LED3"                                                     \
	}
#else
#define APP_NUM_BUTTONS 4
#define APP_NUM_LEDS    4
#define APP_BUTTON_LABELS                                                                          \
	{                                                                                          \
		"Button 1", "Button 2", "Button 3", "Button 4"                                     \
	}
#define APP_LED_LABELS                                                                             \
	{                                                                                          \
		"LED 1", "LED 2", "LED 3", "LED 4"                                                 \
	}
#endif

static inline const char *app_button_label(size_t index)
{
	static const char *const labels[] = APP_BUTTON_LABELS;
	BUILD_ASSERT(ARRAY_SIZE(labels) >= APP_NUM_BUTTONS);
	return (index < ARRAY_SIZE(labels)) ? labels[index] : "Button";
}

static inline const char *app_led_label(size_t index)
{
	static const char *const labels[] = APP_LED_LABELS;
	BUILD_ASSERT(ARRAY_SIZE(labels) >= APP_NUM_LEDS);
	return (index < ARRAY_SIZE(labels)) ? labels[index] : "LED";
}

/* ============================================================================
 * WI-FI MODE  (types now owned by zego/wifi)
 * ============================================================================
 */

#include <wifi.h>

/* enum zego_wifi_mode is defined in zego/bricks/wifi/src/wifi.h (included above).
 * Use ZEGO_WIFI_MODE_STA / SOFTAP / P2P_GO / P2P_GC directly. */

/* ============================================================================
 * DK WIFI INFO MESSAGES
 * ============================================================================
 */

/**
 * @brief Message published by net_event_mgmt when DK Wi-Fi connection is ready.
 */
struct dk_wifi_info_msg {
	enum zego_wifi_mode active_mode; /**< Mode that produced this event */
	char dk_ip_addr[16];            /**< Device IP */
	char dk_mac_addr[18];           /**< Device MAC as XX:XX:XX:XX:XX:XX */
	char ssid[33];                  /**< SoftAP/P2P_GO SSID, or connected AP/GO SSID */
	int error_code;
};

/* ============================================================================
 * APP_WIFI_STATE_CHAN — Wi-Fi connectivity state for LED 0 UX feedback
 *
 * Publisher:  net_event_app.c (defined there via ZBUS_CHAN_DEFINE)
 * Subscriber: src/modules/ux/ux.c (LED 0 state machine)
 * ============================================================================
 */

/** @brief Application-level Wi-Fi state used to drive LED 0. */
enum app_wifi_state {
	/** Boot / connecting — ROTATE on LED 0. */
	APP_WIFI_STATE_CONNECTING = 0,
	/** STA or P2P link established (IP assigned / peer joined). */
	APP_WIFI_STATE_CONNECTED,
	/** SoftAP active — first client connected. */
	APP_WIFI_STATE_SOFTAP,
	/** Link lost or fatal error. */
	APP_WIFI_STATE_ERROR,
};

/** @brief Message published on APP_WIFI_STATE_CHAN. */
struct app_wifi_state_msg {
	enum app_wifi_state state;
	enum zego_wifi_mode mode;
};

ZBUS_CHAN_DECLARE(APP_WIFI_STATE_CHAN);

#endif /* MESSAGES_H */
