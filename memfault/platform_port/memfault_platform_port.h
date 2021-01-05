/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Memfault core interface implementation
 */

#ifndef MEMFAULT_PLATFORM_PORT_H
#define MEMFAULT_PLATFORM_PORT_H
#include "timer.h"

#include "memfault/core/debug_log.h"
#include "memfault/core/event_storage.h"

#ifdef CONFIG_MEMFAULT_PANICS
#include "memfault/panics/coredump.h"
/* This macro takes all possible args and discards the ones we don't use */
#define MEMFAULT_CAPTURE(variable, size)		\
	const struct MfltCoredumpRegion __keep __no_sanitize_address	\
	__memfault_capture_##variable						\
	__attribute__((section(".rodata.mflt"))) =		\
	{ 						\
		.type = kMfltCoredumpRegionType_Memory, \
		.region_start = variable, \
		.region_size = size 	\
	}
#define MEMFAULT_CAPTURE_VALUE(variable)		\
	const struct MfltCoredumpRegion __keep __no_sanitize_address	\
	__memfault_capture_##variable						\
	__attribute__((section(".rodata.mflt"))) =		\
	{ 						\
		.type = kMfltCoredumpRegionType_Memory, \
		.region_start = &variable, \
		.region_size = sizeof(variable) 	\
	}
#else
#define MEMFAULT_CAPTURE(variable, size)
#define MEMFAULT_CAPTURE_VALUE(variable)
#endif

int memfault_platform_metrics_init(const sMemfaultEventStorageImpl *evt_storage);
int memfault_platform_coredump_init(void);

#ifndef ENABLE_MEMFAULT_TIME_MEASURE
#define ENABLE_MEMFAULT_TIME_MEASURE 0
#endif

#if ENABLE_MEMFAULT_TIME_MEASURE

#define MEASURE_TIME(x, description) do { \
	timestamp_t start = get_time();\
	x; \
	uint32_t elapsed = time_since32(start); \
	MEMFAULT_LOG_INFO("%s took %d us",description,elapsed); \
} while(0)

#else

#define MEASURE_TIME(x, description) x

#endif /* ENABLE_MEMFAULT_TIME_MEASURE */

#endif /* MEMFAULT_PLATFORM_PORT_H */