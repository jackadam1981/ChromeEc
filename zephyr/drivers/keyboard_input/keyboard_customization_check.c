/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef CONFIG_KEYBOARD_CUSTOMIZATION

#include "keyboard_customization.h"

#include <zephyr/sys/util_macro.h>
#include <zephyr/toolchain.h>

#if defined(KEYBOARD_COL_DOWN) || defined(KEYBOARD_ROW_DOWN) ||               \
	defined(KEYBOARD_COL_ESC) || defined(KEYBOARD_ROW_ESC) ||             \
	defined(KEYBOARD_COL_KEY_H) || defined(KEYBOARD_ROW_KEY_H) ||         \
	defined(KEYBOARD_COL_KEY_R) || defined(KEYBOARD_ROW_KEY_R) ||         \
	defined(KEYBOARD_COL_LEFT_ALT) || defined(KEYBOARD_ROW_LEFT_ALT) ||   \
	defined(KEYBOARD_COL_REFRESH) || defined(KEYBOARD_ROW_REFRESH) ||     \
	defined(KEYBOARD_COL_RIGHT_ALT) || defined(KEYBOARD_ROW_RIGHT_ALT) || \
	defined(KEYBOARD_DEFAULT_COL_VOL_UP) ||                               \
	defined(KEYBOARD_DEFAULT_ROW_VOL_UP) ||                               \
	defined(KEYBOARD_COL_LEFT_SHIFT) || defined(KEYBOARD_ROW_LEFT_SHIFT)

#error "Do not define KEYBOARD_COL_* and KEYBOARD_ROW_* in" \
	"keyboard_customization.h, use cros-ec,boot-keys and" \
	"cros-ec,runtime-keys instead."

#endif

#endif /* CONFIG_KEYBOARD_CUSTOMIZATION */
