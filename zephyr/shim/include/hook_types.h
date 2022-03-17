/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __CROS_EC_HOOK_TYPES_H_
#define __CROS_EC_HOOK_TYPES_H_

#include <sys/util_macro.h>

/*
 * HOOK_TYPES_LIST is a sequence of tokens that expands to every enabled
 * `enum hook_type` value.
 *
 * If the enum definition is changed, this macro must also be changed.
 */
#define HOOK_TYPES_LIST                                                    \
	LIST_DROP_EMPTY(                                                   \
		HOOK_INIT, HOOK_PRE_FREQ_CHANGE, HOOK_FREQ_CHANGE,         \
		HOOK_SYSJUMP, HOOK_CHIPSET_PRE_INIT, HOOK_CHIPSET_STARTUP, \
		HOOK_CHIPSET_RESUME, HOOK_CHIPSET_SUSPEND,                 \
		IF_ENABLED(CONFIG_CHIPSET_RESUME_INIT_HOOK,                \
			   (HOOK_CHIPSET_RESUME_INIT,                      \
			    HOOK_CHIPSET_SUSPEND_COMPLETE, ))              \
			HOOK_CHIPSET_SHUTDOWN,                             \
		HOOK_CHIPSET_SHUTDOWN_COMPLETE, HOOK_CHIPSET_HARD_OFF,     \
		HOOK_CHIPSET_RESET, HOOK_AC_CHANGE, HOOK_LID_CHANGE,       \
		HOOK_TABLET_MODE_CHANGE, HOOK_BASE_ATTACHED_CHANGE,        \
		HOOK_POWER_BUTTON_CHANGE, HOOK_BATTERY_SOC_CHANGE,         \
		IF_ENABLED(CONFIG_USB_SUSPEND, (HOOK_USB_PM_CHANGE, ))     \
			HOOK_TICK,                                         \
		HOOK_SECOND, HOOK_USB_PD_DISCONNECT, HOOK_USB_PD_CONNECT,  \
		IF_ENABLED(TEST_BUILD,                                     \
			   (HOOK_TEST_1, HOOK_TEST_2, HOOK_TEST_3, )) EMPTY)

BUILD_ASSERT(NUM_VA_ARGS_LESS_1(HOOK_TYPES_LIST) + 1 == HOOK_TYPE_COUNT,
	     "HOOK_TYPES_LIST has diverged from hook_type definition");
#endif
