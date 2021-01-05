/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Memfault log interface implementation
 */

#ifndef MEMFAULT_PLATFORM_LOG_CONFIG_H
#define MEMFAULT_PLATFORM_LOG_CONFIG_H

#include "console.h"

#define MEMFAULT_RAM_BACKED_LOG_SIZE (1024*2)

#ifndef ENABLE_MEMFAULT_LOGS_TO_CONSOLE
#define ENABLE_MEMFAULT_LOGS_TO_CONSOLE 1
#endif

#if ENABLE_MEMFAULT_LOGS_TO_CONSOLE

#define _MEMFAULT_PLATFORM_LOG_IMPL(level, fmt, ...) \
	cprintf(CC_MEMFAULT, #level ": " fmt "\n", ## __VA_ARGS__)

#else

#define _MEMFAULT_PLATFORM_LOG_IMPL(level, ...)

#endif /* ENABLE_MEMFAULT_LOGS_TO_CONSOLE */

#define MEMFAULT_LOG_DEBUG(...) _MEMFAULT_PLATFORM_LOG_IMPL(DEBUG, __VA_ARGS__)
#define MEMFAULT_LOG_INFO(...) _MEMFAULT_PLATFORM_LOG_IMPL(INFO, __VA_ARGS__)
#define MEMFAULT_LOG_WARN(...) _MEMFAULT_PLATFORM_LOG_IMPL(WARN, __VA_ARGS__)
#define MEMFAULT_LOG_ERROR(...) _MEMFAULT_PLATFORM_LOG_IMPL(ERROR, __VA_ARGS__)

#endif /* MEMFAULT_PLATFORM_LOG_CONFIG_H */