/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "button.h"
#include "../messages.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(button_module, CONFIG_BUTTON_MODULE_LOG_LEVEL);

#include <dk_buttons_and_leds.h>
#include <zephyr/kernel.h>
#include <zephyr/smf.h>
#include <zephyr/zbus/zbus.h>

#define NUM_BUTTONS APP_NUM_BUTTONS

/* ============================================================================
 * ZBUS CHANNEL DEFINITION
 * ============================================================================
 */

ZBUS_CHAN_DEFINE(BUTTON_CHAN, struct button_msg, NULL, NULL,
		 ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));

/* ============================================================================
 * STATE MACHINE CONTEXT
 * ============================================================================
 */

/* Forward declarations of state functions */
static enum smf_state_result button_idle_run(void *obj);
static void button_pressed_entry(void *obj);
static enum smf_state_result button_pressed_run(void *obj);
static void button_released_entry(void *obj);

/* State table */
static const struct smf_state button_states[] = {
	[0] = SMF_CREATE_STATE(NULL, button_idle_run, NULL, NULL, NULL),
	[1] = SMF_CREATE_STATE(button_pressed_entry, button_pressed_run, NULL,
			       NULL, NULL),
	[2] = SMF_CREATE_STATE(button_released_entry, NULL, NULL, NULL, NULL),
};

/* Button state machine object */
struct button_sm_object {
	struct smf_ctx ctx;
	uint8_t button_number;
	uint32_t press_count;
	bool current_state;
	bool previous_state;
};

static struct button_sm_object button_sm[NUM_BUTTONS];

/* ============================================================================
 * STATE MACHINE IMPLEMENTATIONS
 * ============================================================================
 */

static enum smf_state_result button_idle_run(void *obj)
{
	struct button_sm_object *sm = (struct button_sm_object *)obj;

	/* Check if button was pressed */
	if (sm->current_state && !sm->previous_state) {
		smf_set_state(SMF_CTX(sm), &button_states[1]);
	}

	sm->previous_state = sm->current_state;

	return SMF_EVENT_HANDLED;
}

static void button_pressed_entry(void *obj)
{
	struct button_sm_object *sm = (struct button_sm_object *)obj;
	struct button_msg msg;

	sm->press_count++;

	msg.type = BUTTON_PRESSED;
	msg.button_number = sm->button_number;
	msg.press_count = sm->press_count;
	msg.timestamp = k_uptime_get_32();

	int ret = zbus_chan_pub(&BUTTON_CHAN, &msg, K_MSEC(100));
	if (ret < 0) {
		LOG_ERR("Failed to publish button pressed event: %d", ret);
	} else {
		const char *label = app_button_label(sm->button_number);
		LOG_INF("%s pressed (count: %d)", label, sm->press_count);
	}
}

static enum smf_state_result button_pressed_run(void *obj)
{
	struct button_sm_object *sm = (struct button_sm_object *)obj;

	/* Check if button was released */
	if (!sm->current_state && sm->previous_state) {
		smf_set_state(SMF_CTX(sm), &button_states[2]);
	}

	sm->previous_state = sm->current_state;

	return SMF_EVENT_HANDLED;
}

static void button_released_entry(void *obj)
{
	struct button_sm_object *sm = (struct button_sm_object *)obj;
	struct button_msg msg;

	msg.type = BUTTON_RELEASED;
	msg.button_number = sm->button_number;
	msg.press_count = sm->press_count;
	msg.timestamp = k_uptime_get_32();

	int ret = zbus_chan_pub(&BUTTON_CHAN, &msg, K_MSEC(100));
	if (ret < 0) {
		LOG_ERR("Failed to publish button released event: %d", ret);
	} else {
		const char *label = app_button_label(sm->button_number);
		LOG_INF("%s released", label);
	}

	/* Return to idle state */
	smf_set_state(SMF_CTX(sm), &button_states[0]);
}

/* ============================================================================
 * BUTTON EVENT HANDLING
 * ============================================================================
 */

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	LOG_DBG("Button handler: state=0x%08x changed=0x%08x", button_state,
		has_changed);

	for (int i = 0; i < NUM_BUTTONS; i++) {
		uint32_t button_mask = BIT(i);

		if (has_changed & button_mask) {
			button_sm[i].current_state =
				(button_state & button_mask) ? true : false;
			LOG_DBG("Button index %d (mask 0x%x) changed: %s", i,
				button_mask,
				button_sm[i].current_state ? "pressed"
							   : "released");

			/* Run state machine */
			int ret = smf_run_state(SMF_CTX(&button_sm[i]));
			if (ret < 0) {
				LOG_ERR("Button SM error: %d", ret);
			}
		}
	}
}

/* ============================================================================
 * BUTTON THREAD
 * ============================================================================
 */

static void button_thread_fn(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	LOG_DBG("Button module thread started!");

	while (1) {
		/* State machines are run from button_handler callback */
		k_sleep(K_MSEC(100));
	}
}

K_THREAD_DEFINE(button_thread_id, 1024, button_thread_fn, NULL, NULL, NULL, 7,
		0, 0);

/* ============================================================================
 * MODULE INITIALIZATION
 * ============================================================================
 */

int button_module_init(void)
{
	int ret;

	LOG_INF("Initializing button module");
	LOG_INF("NUM_BUTTONS = %d", NUM_BUTTONS);

	/* Initialize DK buttons */
	ret = dk_buttons_init(button_handler);
	if (ret) {
		LOG_ERR("Failed to initialize DK buttons: %d", ret);
		return ret;
	}

	LOG_INF("DK buttons initialized successfully");

	/* Initialize state machines for each button */
	for (int i = 0; i < NUM_BUTTONS; i++) {
		button_sm[i].button_number = i;
		button_sm[i].press_count = 0;
		button_sm[i].current_state = false;
		button_sm[i].previous_state = false;

		smf_set_initial(SMF_CTX(&button_sm[i]), &button_states[0]);

		const char *label = app_button_label(i);
		LOG_INF("Initialized button %d: %s", i, label);
	}

	LOG_INF("Button module initialized");

	return 0;
}

SYS_INIT(button_module_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
