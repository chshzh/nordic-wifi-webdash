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
 * WI-FI MODE (v2.0)
 * ============================================================================
 */

/**
 * @brief Wi-Fi operating mode
 */
enum wifi_mode {
	WIFI_MODE_SOFTAP = 0, /**< Device creates its own SoftAP */
	WIFI_MODE_STA = 1,    /**< Device connects to existing SoftAP */
	WIFI_MODE_P2P = 2,    /**< Wi-Fi Direct to phone/peer */
};

/**
 * @brief Wi-Fi mode message (published once at boot by mode_selector)
 */
struct wifi_mode_msg {
	enum wifi_mode mode;
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
 * WIFI MESSAGES (v2.0 — multi-mode)
 * ============================================================================
 */

/**
 * @brief WiFi message types
 */
enum wifi_msg_type {
	WIFI_SOFTAP_STARTED,          /**< SoftAP started, clients may connect */
	WIFI_SOFTAP_STA_CONNECTED,    /**< A client station joined the SoftAP */
	WIFI_SOFTAP_STA_DISCONNECTED, /**< A client station left the SoftAP */
	WIFI_STA_CONNECTED,           /**< STA associated and IP assigned */
	WIFI_STA_DISCONNECTED,        /**< STA lost connection */
	WIFI_P2P_CONNECTED,           /**< P2P group established, IP assigned */
	WIFI_P2P_DISCONNECTED,        /**< P2P group removed */
	WIFI_ERROR,                   /**< WiFi subsystem error */
};

/**
 * @brief WiFi message structure
 */
struct wifi_msg {
	enum wifi_msg_type type;
	enum wifi_mode active_mode; /**< Mode that produced this event */
	char ip_addr[16];           /**< Dotted-decimal IP, filled on connect */
	char ssid[33];              /**< SSID, filled on connect */
	char client_mac[18];        /**< STA MAC "XX:XX:XX:XX:XX:XX", SoftAP only */
	int error_code;
};

/* ============================================================================
 * WEBSERVER MESSAGES (kept for backward compat, unused internally)
 * ============================================================================
 */

/**
 * @brief Webserver message types
 */
enum webserver_msg_type {
	WEBSERVER_STARTED,        /**< Webserver started */
	WEBSERVER_STOPPED,        /**< Webserver stopped */
	WEBSERVER_CLIENT_REQUEST, /**< Client request received */
};

/**
 * @brief Webserver message structure
 */
struct webserver_msg {
	enum webserver_msg_type type;
	uint32_t timestamp;
};

#endif /* MESSAGES_H */
