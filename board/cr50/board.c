/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "task.h"
#include "usb.h"
#include "util.h"

#include "gpio_list.h"

/* Initialize board. */
static void board_init(void)
{
	/* TODO(crosbug.com/p/33812): Try enabling this */
	/* gpio_enable_interrupt(GPIO_CAMO0_BREACH_INT); */
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

const void * const usb_strings[] = {
	[USB_STR_DESC] = usb_string_desc,
	[USB_STR_VENDOR] = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT] = USB_STRING_DESC("Cr50"),
	[USB_STR_VERSION] = USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_CONSOLE_NAME] = USB_STRING_DESC("Shell"),
};
BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);
