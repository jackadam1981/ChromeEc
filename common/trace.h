/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

/* Matches chrome://tracing event types */
enum trace_type {
	TRACE_BEGIN = 'B',
	TRACE_END = 'E',
	TRACE_INSTANT = 'i',
};

#ifdef CONFIG_TRACE
/* Add an event to trace buffer. evt must be allocated statically. */
void add_trace_event(const char *evt, enum trace_type type);
#else
static inline void add_trace_event(const char *evt, enum trace_type type) {}
#endif
