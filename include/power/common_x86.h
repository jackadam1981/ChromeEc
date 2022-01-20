/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel X86 chipset power control module for Chrome EC */


#ifndef __CROS_EC_COMMON_X86_H
#define __CROS_EC_COMMON_X86_H

/**
 * Introduces SYS_RESET_L Debounce time delay
 *
 * The default implementation is to wait for a duration of 32 ms.
 * If a board needs a different debounce time delay, they may override
 * this function
 */
__override_proto void x86_sys_reset_delay(void);

#endif /* __CROS_EC_COMMON_X86_H */
