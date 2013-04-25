/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB charging control module for Chrome EC */

#include "chipset.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "system.h"
#include "usb_charge.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_USBCHARGE, outstr)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static void usb_port_set_all(int en)
{
	gpio_set_level(GPIO_USB1_ENABLE, en);
	gpio_set_level(GPIO_USB2_ENABLE, en);
}
static void usb_port_resume(void)
{
	/* Turn on USB ports on as we go into S0 from S3 or S5. */
	usb_port_set_all(1);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, usb_port_resume, HOOK_PRIO_DEFAULT);

static void usb_port_shutdown(void)
{
	/* Turn on USB ports off as we go back to S5. */
	usb_port_set_all(0);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, usb_port_shutdown, HOOK_PRIO_DEFAULT);
