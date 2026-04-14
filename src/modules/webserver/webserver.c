/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "webserver.h"
#include "../button/button.h"
#include "../led/led.h"
#include "../messages.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(webserver_module, CONFIG_WEBSERVER_MODULE_LOG_LEVEL);

#include <stdio.h>
#include <string.h>
#include <zephyr/data/json.h>
#include <zephyr/kernel.h>
#include <zephyr/net/dns_sd.h>
#include <zephyr/net/http/service.h>
#include <zephyr/net/socket.h>
#include <zephyr/smf.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/zbus/zbus.h>

#define NUM_BUTTONS     APP_NUM_BUTTONS
#define NUM_LEDS        APP_NUM_LEDS
/* Only one HTTP client connects at a time — one WiFi station per mode.
 * Note: a browser still opens several short-lived TCP connections to load
 * the page (HTML + JS + CSS), which is fine with a limit of 1 because
 * those are sequential, not simultaneous.
 */
#define MAX_WEB_CLIENTS 4

BUILD_ASSERT(NUM_BUTTONS > 0, "At least one button expected");
BUILD_ASSERT(NUM_LEDS > 0, "At least one LED expected");
BUILD_ASSERT(MAX_WEB_CLIENTS > 0, "At least one web client must be allowed");

/* ============================================================================
 * SYSTEM STATE CACHE (updated from CLIENT_CONNECTED_CHAN events)
 * ============================================================================
 */

static char cached_mode_str[12] = "SoftAP";
static char cached_dk_ip[16] = "0.0.0.0";
static char cached_dk_mac[18] = "00:00:00:00:00:00";
static char cached_ssid[33] = "";
static char cached_client_ip[16] = "0.0.0.0";
static bool server_started;

static void update_cached_client_ip_from_http(const struct http_client_ctx *client)
{
	if (client == NULL || client->fd < 0) {
		return;
	}

	struct sockaddr_storage peer_addr = {0};
	socklen_t peer_len = sizeof(peer_addr);

	if (zsock_getpeername(client->fd, (struct sockaddr *)&peer_addr, &peer_len) < 0) {
		return;
	}

	char peer_ip[INET6_ADDRSTRLEN] = {0};

	if (peer_addr.ss_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in *)&peer_addr;

		zsock_inet_ntop(AF_INET, &sin->sin_addr, peer_ip, sizeof(peer_ip));
	} else if (peer_addr.ss_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&peer_addr;

		zsock_inet_ntop(AF_INET6, &sin6->sin6_addr, peer_ip, sizeof(peer_ip));
	}

	if (peer_ip[0] != '\0') {
		snprintf(cached_client_ip, sizeof(cached_client_ip), "%s", peer_ip);
	}
}

/* ============================================================================
 * WIFI READY EVENT LISTENER -> starts HTTP server when connection is ready
 * ============================================================================
 */

extern const struct zbus_channel CLIENT_CONNECTED_CHAN;

/* ============================================================================
 * BUTTON STATE TRACKING (via Zbus)
 * ============================================================================
 */

struct button_state {
	bool is_pressed;
	uint32_t press_count;
};

static struct button_state button_states[NUM_BUTTONS];

static void button_listener(const struct zbus_channel *chan)
{
	const struct button_msg *msg = zbus_chan_const_msg(chan);

	if (msg->button_number < NUM_BUTTONS) {
		int idx = msg->button_number;

		button_states[idx].press_count = msg->press_count;
		button_states[idx].is_pressed = (msg->type == BUTTON_PRESSED);

		LOG_DBG("Button %d state updated: %s, count=%d", msg->button_number,
			button_states[idx].is_pressed ? "pressed" : "released",
			button_states[idx].press_count);
	}
}

ZBUS_LISTENER_DEFINE(button_listener_def, button_listener);

/* ============================================================================
 * HTTP CLIENT EVENT LISTENER -> starts HTTP server when a WiFi client connects
 * ============================================================================
 */

/* Deferred work: start the HTTP server from the system work queue after a
 * short delay that lets DHCP/IP settle.  Using k_work_delayable here instead
 * of k_sleep() inside the zbus listener is mandatory: listeners execute
 * synchronously in the publisher's thread context (VDED), so sleeping there
 * would block the net_mgmt event thread for an entire second.
 */
static void webserver_start_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	int ret = webserver_start();

	if (ret < 0) {
		LOG_ERR("Failed to start webserver: %d", ret);
	}
}
static K_WORK_DELAYABLE_DEFINE(webserver_start_work, webserver_start_work_fn);

static void http_client_event_listener(const struct zbus_channel *chan)
{
	const struct dk_wifi_info_msg *msg = zbus_chan_const_msg(chan);

	/* Determine server IP and mode string from the active mode */
	switch (msg->active_mode) {
	case APP_WIFI_MODE_SOFTAP:
		snprintf(cached_mode_str, sizeof(cached_mode_str), "SoftAP");
		break;
	case APP_WIFI_MODE_STA:
		snprintf(cached_mode_str, sizeof(cached_mode_str), "STA");
		break;
	case APP_WIFI_MODE_P2P_GO:
		snprintf(cached_mode_str, sizeof(cached_mode_str), "P2P_GO");
		break;
	case APP_WIFI_MODE_P2P_CLIENT:
		snprintf(cached_mode_str, sizeof(cached_mode_str), "P2P_CLIENT");
		break;
	default:
		return;
	}

	snprintf(cached_dk_ip, sizeof(cached_dk_ip), "%s", msg->dk_ip_addr);
	snprintf(cached_dk_mac, sizeof(cached_dk_mac), "%s", msg->dk_mac_addr);
	snprintf(cached_ssid, sizeof(cached_ssid), "%s", msg->ssid);

	LOG_INF("CLIENT_CONNECTED_CHAN -> mode=%s server_ip=%s client_ip=%s ssid=%s",
		cached_mode_str, cached_dk_ip, cached_client_ip, cached_ssid);

	/* Schedule HTTP server start the first time a connected event arrives. */
	if (!server_started) {
		k_work_schedule(&webserver_start_work, K_NO_WAIT);
	}
}

ZBUS_LISTENER_DEFINE(http_client_listener_def, http_client_event_listener);
ZBUS_CHAN_ADD_OBS(CLIENT_CONNECTED_CHAN, http_client_listener_def, 0);

/* ============================================================================
 * HTTP SERVICE DEFINITION
 * ============================================================================
 */

extern const struct zbus_channel BUTTON_CHAN;
extern const struct zbus_channel LED_CMD_CHAN;
ZBUS_CHAN_ADD_OBS(BUTTON_CHAN, button_listener_def, 0);

static uint16_t http_service_port = CONFIG_APP_HTTP_PORT;

/* DNS-SD / mDNS: advertise "_http._tcp.local" so browsers and apps can
 * discover the dashboard via service discovery on all Wi-Fi modes
 * (SoftAP, STA, P2P).  DNS-SD requires the port in network byte order.
 */
static const uint16_t http_dns_sd_port = sys_cpu_to_be16(CONFIG_APP_HTTP_PORT);

DNS_SD_REGISTER_SERVICE(webdash_http, CONFIG_NET_HOSTNAME, "_http", "_tcp", "local",
			DNS_SD_EMPTY_TXT, &http_dns_sd_port);

HTTP_SERVICE_DEFINE(webserver_service, NULL, &http_service_port, MAX_WEB_CLIENTS, MAX_WEB_CLIENTS,
		    NULL, NULL, NULL);

/* ============================================================================
 * STATIC WEB RESOURCES
 * ============================================================================
 */

/* Index HTML */
static const uint8_t index_html_gz[] = {
#include "index.html.gz.inc"
};

struct http_resource_detail_static index_html_resource_detail = {
	/* clang-format off */
	.common = {
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_encoding = "gzip",
			.content_type = "text/html",
		},
	/* clang-format on */
	.static_data = index_html_gz,
	.static_data_len = sizeof(index_html_gz),
};

HTTP_RESOURCE_DEFINE(index_html_resource, webserver_service, "/", &index_html_resource_detail);

/* Main JS */
static const uint8_t main_js_gz[] = {
#include "main.js.gz.inc"
};

struct http_resource_detail_static main_js_resource_detail = {
	/* clang-format off */
	.common = {
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_encoding = "gzip",
			.content_type = "application/javascript",
		},
	/* clang-format on */
	.static_data = main_js_gz,
	.static_data_len = sizeof(main_js_gz),
};

HTTP_RESOURCE_DEFINE(main_js_resource, webserver_service, "/main.js", &main_js_resource_detail);

/* Styles CSS */
static const uint8_t styles_css_gz[] = {
#include "styles.css.gz.inc"
};

struct http_resource_detail_static styles_css_resource_detail = {
	/* clang-format off */
	.common = {
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_encoding = "gzip",
			.content_type = "text/css",
		},
	/* clang-format on */
	.static_data = styles_css_gz,
	.static_data_len = sizeof(styles_css_gz),
};

HTTP_RESOURCE_DEFINE(styles_css_resource, webserver_service, "/styles.css",
		     &styles_css_resource_detail);

/* ============================================================================
 * GET /api/system
 * ============================================================================
 */

static uint8_t system_api_buf[512];

static int system_api_handler(struct http_client_ctx *client, enum http_transaction_status status,
			      const struct http_request_ctx *request_ctx,
			      struct http_response_ctx *response_ctx, void *user_data)
{
	ARG_UNUSED(client);
	ARG_UNUSED(request_ctx);
	ARG_UNUSED(user_data);

	if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
		return 0;
	}

	update_cached_client_ip_from_http(client);

	uint32_t uptime_s = (uint32_t)(k_uptime_get() / 1000);

	int len = snprintf((char *)system_api_buf, sizeof(system_api_buf),
			   "{\"mode\":\"%s\","
			   "\"device_ip\":\"%s\","
			   "\"device_mac\":\"%s\","
			   "\"client_ip\":\"%s\","
			   "\"ssid\":\"%s\","
			   "\"uptime_s\":%u,"
			   "\"board\":\"%s\"}",
			   cached_mode_str, cached_dk_ip, cached_dk_mac, cached_client_ip,
			   cached_ssid, uptime_s, CONFIG_BOARD);

	if (len <= 0 || len >= (int)sizeof(system_api_buf)) {
		return -ENOMEM;
	}

	LOG_INF("GET /api/system -> mode=%s device_ip=%s client_ip=%s", cached_mode_str,
		cached_dk_ip, cached_client_ip);

	response_ctx->body = system_api_buf;
	response_ctx->body_len = len;
	response_ctx->final_chunk = true;
	response_ctx->status = HTTP_200_OK;

	return 0;
}

static struct http_resource_detail_dynamic system_api_detail = {
	/* clang-format off */
	.common = {
			.type = HTTP_RESOURCE_TYPE_DYNAMIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_type = "application/json",
		},
	/* clang-format on */
	.cb = system_api_handler,
	.holder = NULL,
	.user_data = NULL,
};

HTTP_RESOURCE_DEFINE(system_api_resource, webserver_service, "/api/system", &system_api_detail);

/* ============================================================================
 * GET /api/buttons
 * ============================================================================
 */

static uint8_t button_api_buf[512];

static int button_api_handler(struct http_client_ctx *client, enum http_transaction_status status,
			      const struct http_request_ctx *request_ctx,
			      struct http_response_ctx *response_ctx, void *user_data)
{
	ARG_UNUSED(client);
	ARG_UNUSED(request_ctx);
	ARG_UNUSED(user_data);

	if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
		return 0;
	}

	update_cached_client_ip_from_http(client);

	int offset = 0;
	int remaining = sizeof(button_api_buf);
	int written = snprintf((char *)button_api_buf + offset, remaining, "{\"buttons\":[");

	if (written < 0 || written >= remaining) {
		return -ENOMEM;
	}
	offset += written;
	remaining -= written;

	for (int i = 0; i < NUM_BUTTONS; i++) {
		const bool is_last = (i == NUM_BUTTONS - 1);
		const uint8_t btn_num = (uint8_t)i;
		const char *btn_name = app_button_label(btn_num);

		written = snprintf((char *)button_api_buf + offset, remaining,
				   /* clang-format off */
			"{\"number\":%u,\"name\":\"%s\",\"pressed\":%s,\"count\":%u}%s",
				   /* clang-format on */
				   btn_num, btn_name ? btn_name : "",
				   button_states[i].is_pressed ? "true" : "false",
				   button_states[i].press_count, is_last ? "" : ",");
		if (written < 0 || written >= remaining) {
			return -ENOMEM;
		}
		offset += written;
		remaining -= written;
	}

	written = snprintf((char *)button_api_buf + offset, remaining, "]}");
	if (written < 0 || written >= remaining) {
		return -ENOMEM;
	}
	offset += written;

	response_ctx->body = button_api_buf;
	response_ctx->body_len = offset;
	response_ctx->final_chunk = true;
	response_ctx->status = HTTP_200_OK;

	return 0;
}

static struct http_resource_detail_dynamic button_api_detail = {
	/* clang-format off */
	.common = {
			.type = HTTP_RESOURCE_TYPE_DYNAMIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_type = "application/json",
		},
	/* clang-format on */
	.cb = button_api_handler,
	.holder = NULL,
	.user_data = NULL,
};

HTTP_RESOURCE_DEFINE(button_api_resource, webserver_service, "/api/buttons", &button_api_detail);

/* ============================================================================
 * GET /api/leds
 * ============================================================================
 */

static uint8_t led_get_api_buf[512];

static int led_get_api_handler(struct http_client_ctx *client, enum http_transaction_status status,
			       const struct http_request_ctx *request_ctx,
			       struct http_response_ctx *response_ctx, void *user_data)
{
	ARG_UNUSED(client);
	ARG_UNUSED(request_ctx);
	ARG_UNUSED(user_data);

	if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
		return 0;
	}

	update_cached_client_ip_from_http(client);

	int written = led_get_all_states_json((char *)led_get_api_buf, sizeof(led_get_api_buf));

	if (written > 0) {
		response_ctx->body = led_get_api_buf;
		response_ctx->body_len = written;
		response_ctx->final_chunk = true;
		response_ctx->status = HTTP_200_OK;
	}

	return 0;
}

static struct http_resource_detail_dynamic led_get_api_detail = {
	/* clang-format off */
	.common = {
			.type = HTTP_RESOURCE_TYPE_DYNAMIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_type = "application/json",
		},
	/* clang-format on */
	.cb = led_get_api_handler,
	.holder = NULL,
	.user_data = NULL,
};

HTTP_RESOURCE_DEFINE(led_get_api_resource, webserver_service, "/api/leds", &led_get_api_detail);

/* ============================================================================
 * POST /api/led
 * ============================================================================
 */

struct led_control_cmd {
	uint8_t led;
	char action[8]; /* "on", "off", "toggle" */
};

static const struct json_obj_descr led_control_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct led_control_cmd, led, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct led_control_cmd, action, JSON_TOK_STRING_BUF),
};

static int led_post_api_handler(struct http_client_ctx *client, enum http_transaction_status status,
				const struct http_request_ctx *request_ctx,
				struct http_response_ctx *response_ctx, void *user_data)
{
	ARG_UNUSED(client);
	ARG_UNUSED(user_data);

	if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
		return 0;
	}

	update_cached_client_ip_from_http(client);

	if (request_ctx->data == NULL || request_ctx->data_len == 0) {
		response_ctx->status = HTTP_400_BAD_REQUEST;
		response_ctx->final_chunk = true;
		return 0;
	}

	struct led_control_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	int ret = json_obj_parse((char *)request_ctx->data, request_ctx->data_len,
				 led_control_descr, ARRAY_SIZE(led_control_descr), &cmd);

	if (ret < 0) {
		LOG_WRN("Failed to parse LED command: %d", ret);
		response_ctx->status = HTTP_400_BAD_REQUEST;
		response_ctx->final_chunk = true;
		return 0;
	}

	LOG_INF("POST /api/led -> LED %d action='%s'", cmd.led, cmd.action);

	if (cmd.led >= NUM_LEDS) {
		LOG_WRN("LED index out of range: %d (max %d)", cmd.led, NUM_LEDS - 1);
		response_ctx->status = HTTP_400_BAD_REQUEST;
		response_ctx->final_chunk = true;
		return 0;
	}

	struct led_msg msg;

	msg.led_number = cmd.led;

	if (strcmp(cmd.action, "on") == 0) {
		msg.type = LED_COMMAND_ON;
	} else if (strcmp(cmd.action, "off") == 0) {
		msg.type = LED_COMMAND_OFF;
	} else if (strcmp(cmd.action, "toggle") == 0) {
		msg.type = LED_COMMAND_TOGGLE;
	} else {
		LOG_WRN("Unknown LED action: %s", cmd.action);
		response_ctx->status = HTTP_400_BAD_REQUEST;
		response_ctx->final_chunk = true;
		return 0;
	}

	ret = zbus_chan_pub(&LED_CMD_CHAN, &msg, K_MSEC(100));
	if (ret < 0) {
		LOG_ERR("Failed to publish LED command: %d", ret);
		response_ctx->status = HTTP_500_INTERNAL_SERVER_ERROR;
	} else {
		response_ctx->status = HTTP_200_OK;
	}

	response_ctx->final_chunk = true;
	return 0;
}

static struct http_resource_detail_dynamic led_post_api_detail = {
	/* clang-format off */
	.common = {
			.type = HTTP_RESOURCE_TYPE_DYNAMIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		},
	/* clang-format on */
	.cb = led_post_api_handler,
	.holder = NULL,
	.user_data = NULL,
};

HTTP_RESOURCE_DEFINE(led_post_api_resource, webserver_service, "/api/led", &led_post_api_detail);

/* ============================================================================
 * PUBLIC API
 * ============================================================================
 */

int webserver_start(void)
{
	if (server_started) {
		LOG_DBG("HTTP server already started");
		return 0;
	}

	LOG_INF("Starting HTTP server on port %d (mode=%s device_ip=%s)", CONFIG_APP_HTTP_PORT,
		cached_mode_str, cached_dk_ip);

	int ret = http_server_start();

	if (ret < 0) {
		LOG_ERR("Failed to start HTTP server: %d", ret);
		return ret;
	}

	server_started = true;
	LOG_INF("HTTP server started -> http://%s:%d or http://%s.local:%d", cached_dk_ip,
		CONFIG_APP_HTTP_PORT, CONFIG_NET_HOSTNAME, CONFIG_APP_HTTP_PORT);

	return 0;
}
