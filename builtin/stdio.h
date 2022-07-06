/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STDIO_H__
#define __CROS_EC_STDIO_H__

#include <stddef.h>
#include <stdarg.h>

#if defined(CONFIG_ZEPHYR)
#define EC_SNPRINTF crec_snprintf
#define EC_VSNPRINTF crec_vsnprintf
#else
#define EC_SNPRINTF snprintf
#define EC_VSNPRINTF vsnprintf
#endif

/**
 * Print formatted output to a string.
 *
 * Guarantees null-termination if size!=0.
 *
 * @param str		Destination string
 * @param size		Size of destination in bytes
 * @param format	Format string
 * @return EC_SUCCESS, or EC_ERROR_OVERFLOW if the output was truncated.
 */
__attribute__((__format__(__printf__, 3, 4))) int
EC_SNPRINTF(char *str, size_t size, const char *format, ...);

/**
 * Print formatted output to a string.
 *
 * Guarantees null-termination if size!=0.
 *
 * @param str		Destination string
 * @param size		Size of destination in bytes
 * @param format	Format string
 * @param args		Parameters
 * @return The string length written to str, or a negative value on error.
 *         The negative values can be -EC_ERROR_INVAL or -EC_ERROR_OVERFLOW.
 */
int EC_VSNPRINTF(char *str, size_t size, const char *format, va_list args);

#endif /* __CROS_EC_STDIO_H__ */
