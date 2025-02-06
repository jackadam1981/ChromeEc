/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_PANIC_LOG_H
#define __CROS_EC_PANIC_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

void panic_log_init(void);

void panic_log_write_char(const char c);

#ifdef __cplusplus
}
#endif

#endif /* __CROS_EC_PANIC_LOG_H */