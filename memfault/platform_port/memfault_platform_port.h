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

int memfault_platform_metrics_init(const sMemfaultEventStorageImpl *evt_storage);
int memfault_platform_coredump_init(void);

#undef ENABLE_TIME_MEASURE

#ifdef ENABLE_TIME_MEASURE
#define MEASURE_TIME(x, description) do { \
	timestamp_t start = get_time();\
	x; \
	uint32_t elapsed = time_since32(start); \
	MEMFAULT_LOG_INFO("%s took %d us",description,elapsed); \
} while(0)

#define MEASURE_TIME_RV(x, rv, description) do { \
	timestamp_t start = get_time();\
	rv = x; \
	uint32_t elapsed = time_since32(start); \
	MEMFAULT_LOG_INFO("%s took %d us",description,elapsed); \
} while(0)
#else
#define MEASURE_TIME(x, description) x
#define MEASURE_TIME_RV(x, rv, description) rv = x
#endif


#endif /* MEMFAULT_PLATFORM_PORT_H */