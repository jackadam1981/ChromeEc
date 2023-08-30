/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef TRACE_H
#define TRACE_H

#include "serial.h"

#include <stdarg.h>

void trace_init(void);
void tracex(const char *fmt, ...);

/* output debug statements to UART port 115200,8bits,NP1SB */
#define CONFIG_ENABLE_TRACE 1

#if CONFIG_ENABLE_TRACE

#define TRACE_INIT() trace_init()
#define TRACEX(...) tracex(__VA_ARGS__)

#define TR_NLINE tracex("\r\n")

#define trace0(nbr, cat, lvl, str) \
	do {                       \
		tracex(str);       \
		TR_NLINE;          \
	} while (0)
#define trace1(nbr, cat, lvl, str, p1) \
	do {                           \
		tracex(str, p1);       \
		TR_NLINE;              \
	} while (0)
#define trace2(nbr, cat, lvl, str, p1, p2) \
	do {                               \
		tracex(str, p1, p2);       \
		TR_NLINE;                  \
	} while (0)
#else

#define TRACE_INIT()
#define TRACEX(...)

#define trace0(nbr, cat, lvl, str)
#define trace1(nbr, cat, lvl, str, p1)
#define trace2(nbr, cat, lvl, str, p1, p2)

#endif
#endif /* #ifndef TRACE_H */
