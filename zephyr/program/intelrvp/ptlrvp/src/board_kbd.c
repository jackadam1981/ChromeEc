/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "keyboard_raw.h"

#ifdef CONFIG_PLATFORM_EC_KEYBOARD_DISCRETE
/******************************************************************************/
/* KSO mapping for discrete keyboard */
__override const uint8_t it8801_kso_mapping[] = {
	0, 1, 20, 3, 4, 5, 6, 11, 12, 13, 14, 15, 16,
};
BUILD_ASSERT(ARRAY_SIZE(it8801_kso_mapping) == KEYBOARD_COLS_MAX);
#endif

#ifndef CONFIG_PLATFORM_EC_KEYBOARD
/* Placeholder function to avoid the build errors */
void button_interrupt(enum gpio_signal signal)
{
}
#endif
