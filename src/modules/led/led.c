/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "led.h"
#include "../messages.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(led_module, CONFIG_LED_MODULE_LOG_LEVEL);

#include <dk_buttons_and_leds.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/smf.h>
#include <zephyr/zbus/zbus.h>

/* Number of LEDs available on current board */
#define NUM_LEDS APP_NUM_LEDS

/* ============================================================================
 * ZBUS CHANNEL DEFINITIONS
 * ============================================================================
 */

/* LED command channel (subscribe to this) */
ZBUS_CHAN_DEFINE(LED_CMD_CHAN, struct led_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));

/* LED state channel (publish to this) */
ZBUS_CHAN_DEFINE(LED_STATE_CHAN, struct led_state_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(0));

/* ============================================================================
 * STATE MACHINE CONTEXT
 * ============================================================================
 */

/* Forward declarations */
static void led_on_entry(void *obj);
static enum smf_state_result led_on_run(void *obj);
static void led_off_entry(void *obj);
static enum smf_state_result led_off_run(void *obj);

/* State table */
static const struct smf_state led_states[] = {
	[0] = SMF_CREATE_STATE(led_off_entry, led_off_run, NULL, NULL, NULL),
	[1] = SMF_CREATE_STATE(led_on_entry, led_on_run, NULL, NULL, NULL),
};

/* LED state machine object */
struct led_sm_object {
	struct smf_ctx ctx;
	uint8_t led_number;
	bool is_on;
	enum led_msg_type pending_command;
	bool has_pending_command;
};

static struct led_sm_object led_sm[NUM_LEDS];

/* ============================================================================
 * STATE MACHINE IMPLEMENTATIONS
 * ============================================================================
 */

static void led_off_entry(void *obj)
{
	struct led_sm_object *sm = (struct led_sm_object *)obj;
	struct led_state_msg state_msg;

	/* Turn LED off */
	dk_set_led_off(sm->led_number);
	sm->is_on = false;

	const char *label = app_led_label(sm->led_number);
	LOG_DBG("%s turned OFF", label);

	/* Publish state */
	state_msg.led_number = sm->led_number;
	state_msg.is_on = false;
	zbus_chan_pub(&LED_STATE_CHAN, &state_msg, K_NO_WAIT);
}

static enum smf_state_result led_off_run(void *obj)
{
	struct led_sm_object *sm = (struct led_sm_object *)obj;

	if (sm->has_pending_command) {
		sm->has_pending_command = false;

		if (sm->pending_command == LED_COMMAND_ON ||
		    sm->pending_command == LED_COMMAND_TOGGLE) {
			smf_set_state(SMF_CTX(sm), &led_states[1]);
		}
	}

	return SMF_EVENT_HANDLED;
}

static void led_on_entry(void *obj)
{
	struct led_sm_object *sm = (struct led_sm_object *)obj;
	struct led_state_msg state_msg;

	/* Turn LED on */
	dk_set_led_on(sm->led_number);
	sm->is_on = true;

	const char *label = app_led_label(sm->led_number);
	LOG_DBG("%s turned ON", label);

	/* Publish state */
	state_msg.led_number = sm->led_number;
	state_msg.is_on = true;
	zbus_chan_pub(&LED_STATE_CHAN, &state_msg, K_NO_WAIT);
}

static enum smf_state_result led_on_run(void *obj)
{
	struct led_sm_object *sm = (struct led_sm_object *)obj;

	if (sm->has_pending_command) {
		sm->has_pending_command = false;

		if (sm->pending_command == LED_COMMAND_OFF ||
		    sm->pending_command == LED_COMMAND_TOGGLE) {
			smf_set_state(SMF_CTX(sm), &led_states[0]);
		}
	}

	return SMF_EVENT_HANDLED;
}

/* ============================================================================
 * ZBUS LISTENER
 * ============================================================================
 */

static void led_cmd_listener(const struct zbus_channel *chan)
{
	const struct led_msg *msg = zbus_chan_const_msg(chan);

	if (msg->led_number >= NUM_LEDS) {
		LOG_WRN("Invalid LED number: %d (max: %d)", msg->led_number, NUM_LEDS - 1);
		return;
	}

	struct led_sm_object *sm = &led_sm[msg->led_number];

	sm->pending_command = msg->type;
	sm->has_pending_command = true;

	/* Run state machine */
	int ret = smf_run_state(SMF_CTX(sm));
	if (ret < 0) {
		LOG_ERR("LED SM error: %d", ret);
	}
}

ZBUS_LISTENER_DEFINE(led_cmd_listener_def, led_cmd_listener);
ZBUS_CHAN_ADD_OBS(LED_CMD_CHAN, led_cmd_listener_def, 0);

/* ============================================================================
 * PUBLIC API
 * ============================================================================
 */

int led_get_state(uint8_t led_number, bool *state)
{
	if (led_number >= NUM_LEDS || !state) {
		return -EINVAL;
	}

	*state = led_sm[led_number].is_on;
	return 0;
}

int led_get_all_states_json(char *buf, size_t buf_len)
{
	if (!buf || buf_len == 0) {
		return -EINVAL;
	}

	int offset = 0;
	int remaining = buf_len;
	int written = snprintf(buf, remaining, "{\"leds\":[");
	if (written < 0 || written >= remaining) {
		return -ENOMEM;
	}
	offset += written;
	remaining -= written;

	for (int i = 0; i < NUM_LEDS; i++) {
		bool is_last = (i == NUM_LEDS - 1);
		const char *led_name = app_led_label(i);
		written = snprintf(buf + offset, remaining,
				   "{\"number\":%d,\"name\":\"%s\",\"is_on\":%s}%s", i,
				   led_name ? led_name : "", led_sm[i].is_on ? "true" : "false",
				   is_last ? "" : ",");
		if (written < 0 || written >= remaining) {
			return -ENOMEM;
		}
		offset += written;
		remaining -= written;
	}

	written = snprintf(buf + offset, remaining, "]}");
	if (written < 0 || written >= remaining) {
		return -ENOMEM;
	}
	return offset + written;
}

/* ============================================================================
 * MODULE INITIALIZATION
 * ============================================================================
 */

int led_module_init(void)
{
	int ret;

	LOG_INF("Initializing LED module");

	/* Initialize DK LEDs */
	ret = dk_leds_init();
	if (ret) {
		LOG_ERR("Failed to initialize DK LEDs: %d", ret);
		return ret;
	}

	/* Initialize state machines for each LED */
	for (int i = 0; i < NUM_LEDS; i++) {
		led_sm[i].led_number = i;
		led_sm[i].is_on = false;
		led_sm[i].has_pending_command = false;

		smf_set_initial(SMF_CTX(&led_sm[i]), &led_states[0]);

		/* Run initial state */
		smf_run_state(SMF_CTX(&led_sm[i]));
	}

	LOG_INF("LED module initialized");

	return 0;
}

SYS_INIT(led_module_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
