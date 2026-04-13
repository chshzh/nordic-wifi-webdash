// SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/heap_listener.h>
#include <zephyr/sys/mem_stats.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(app_heap_monitor, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * _system_heap is defined by Zephyr kernel (kernel/mempool.c) but intentionally
 * not exported in a public header. Direct access is needed to monitor the
 * system heap via sys_heap_runtime_stats_get() and HEAP_ID_FROM_POINTER()
 * macros.
 */
extern struct k_heap _system_heap;

static uint32_t last_reported_high;
static uint32_t last_warn_pct;
static bool boot_report_pending = true;

static void heap_report(const char *trigger)
{
	struct sys_memory_stats stats;

	if (sys_heap_runtime_stats_get((struct sys_heap *)&_system_heap.heap, &stats) != 0) {
		return;
	}

	const uint32_t total = (uint32_t)(stats.allocated_bytes + stats.free_bytes);
	if (total == 0U) {
		return;
	}

	const uint32_t current_high = (uint32_t)stats.max_allocated_bytes;
	const uint32_t used = (uint32_t)stats.allocated_bytes;
	const uint32_t free_bytes = (uint32_t)stats.free_bytes;
	const uint32_t pct = (current_high * 100U) / total;
	const bool warn = pct >= CONFIG_APP_HEAP_MONITOR_WARN_PCT;
	bool progressed = false;

	if (current_high > last_reported_high) {
		progressed =
			(current_high - last_reported_high) >= CONFIG_APP_HEAP_MONITOR_STEP_BYTES;
	}

	const bool new_warn = warn && (pct > last_warn_pct);
	const bool should_report = progressed || new_warn || boot_report_pending;
	if (!should_report) {
		return;
	}

	last_reported_high = MAX(last_reported_high, current_high);
	boot_report_pending = false;

	if (warn) {
		last_warn_pct = pct;
		LOG_WRN("Heap %s: peak=%u bytes (%u%% of %u), used=%u, free=%u", trigger,
			current_high, pct, total, used, free_bytes);
	} else {
		LOG_INF("Heap %s: peak=%u bytes (%u%% of %u), used=%u, free=%u", trigger,
			current_high, pct, total, used, free_bytes);
	}
}

static void heap_listener_alloc(uintptr_t heap_id, void *mem, size_t bytes)
{
	ARG_UNUSED(mem);
	ARG_UNUSED(bytes);

	if (heap_id != HEAP_ID_FROM_POINTER(&_system_heap)) {
		return;
	}

	heap_report("alloc");
}

HEAP_LISTENER_ALLOC_DEFINE(app_heap_listener_alloc, HEAP_ID_FROM_POINTER(&_system_heap),
			   heap_listener_alloc);

static int app_heap_monitor_init(void)
{
	heap_report("boot");
	return 0;
}

SYS_INIT(app_heap_monitor_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
