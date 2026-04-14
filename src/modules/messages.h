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
 * WI-FI MODE
 * ============================================================================
 */

/**
 * @brief Wi-Fi operating mode (application-specific enum to avoid conflicts with Zephyr)
 */
enum app_wifi_mode {
	APP_WIFI_MODE_SOFTAP = 0,     /**< Device creates its own SoftAP */
	APP_WIFI_MODE_STA = 1,        /**< Device connects to existing SoftAP */
	APP_WIFI_MODE_P2P_GO = 2,     /**< Wi-Fi Direct — device is Group Owner */
	APP_WIFI_MODE_P2P_CLIENT = 3, /**< Wi-Fi Direct — device joins phone's group */
};

/**
 * @brief Wi-Fi mode message (published once at boot by mode_selector)
 */
struct wifi_mode_msg {
	enum app_wifi_mode mode;
};

/* ============================================================================
 * BUTTON MESSAGES
 * ============================================================================
 */

/**
 * @brief Button message types
 */
enum button_msg_type {
	BUTTON_PRESSED,  /**< Button pressed */
	BUTTON_RELEASED, /**< Button released */
};

/**
 * @brief Button message structure
 */
struct button_msg {
	enum button_msg_type type;
	uint8_t button_number;
	uint32_t press_count; /**< Total number of presses */
	uint32_t timestamp;
};

/* ============================================================================
 * LED MESSAGES
 * ============================================================================
 */

/**
 * @brief LED message types
 */
enum led_msg_type {
	LED_COMMAND_ON,     /**< Turn LED on */
	LED_COMMAND_OFF,    /**< Turn LED off */
	LED_COMMAND_TOGGLE, /**< Toggle LED */
};

/**
 * @brief LED message structure
 */
struct led_msg {
	enum led_msg_type type;
	uint8_t led_number;
};

/* ============================================================================
 * LED STATE MESSAGES (for status reporting)
 * ============================================================================
 */

/**
 * @brief LED state message structure
 */
struct led_state_msg {
	uint8_t led_number;
	bool is_on;
};

/* ============================================================================
 * DK WIFI INFO MESSAGES
 * ============================================================================
 */

/**
 * @brief Message published by net_event_mgmt when DK Wi-Fi connection is ready.
 */
struct dk_wifi_info_msg {
	enum app_wifi_mode active_mode; /**< Mode that produced this event */
	char dk_ip_addr[16];            /**< Device IP */
	char dk_mac_addr[18];           /**< Device MAC as XX:XX:XX:XX:XX:XX */
	char ssid[33];                  /**< SoftAP/P2P_GO SSID, or connected AP/GO SSID */
	int error_code;
};

#endif /* MESSAGES_H */
