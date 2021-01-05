/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Memfault log interface implementation
 */

#include "memfault/core/platform/debug_log.h"

#include <console.h>
#include <stdarg.h>
#include <string.h>
#include <printf.h>

#ifndef MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES
#  define MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES (128)
#endif

static const char *s_log_prefix = "MFLT";

static const char prv_level_to_c(eMemfaultPlatformLogLevel level) {
  switch (level) {
    case kMemfaultPlatformLogLevel_Debug:
      return 'D';
    case kMemfaultPlatformLogLevel_Info:
      return 'I';
    case kMemfaultPlatformLogLevel_Warning:
      return 'W';
    case kMemfaultPlatformLogLevel_Error:
      return 'E';
    default:
      return '?';
  }
}

void memfault_platform_log(eMemfaultPlatformLogLevel level, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  const char level_char = prv_level_to_c(level);

  char log_buf[MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES];
  vsnprintf(log_buf, sizeof(log_buf), fmt, args);

  cprintf(CC_MEMFAULT, "%c/%s: %s\n", level_char, s_log_prefix, log_buf);
  cflush();

  va_end(args);
}

void memfault_platform_log_raw(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  char log_buf[MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES];
  vsnprintf(log_buf, sizeof(log_buf), fmt, args);

  cputs(CC_MEMFAULT, log_buf);
  cflush();

  va_end(args);
}
