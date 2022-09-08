/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __TEST_DRIVERS_SYSTEM_FAKE_H
#define __TEST_DRIVERS_SYSTEM_FAKE_H

#include <setjmp.h>

/**
 * @brief Set the place to jump to on EC reboot
 *
 * @param env Pointer to jump buffer produced by setjmp(), or NULL to do
 * nothing on reboot (no jump)
 */
void system_fake_setenv(jmp_buf *env);

#endif /* __TEST_DRIVERS_SYSTEM_FAKE_H */
