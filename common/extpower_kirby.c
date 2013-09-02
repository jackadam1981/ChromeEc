/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB charging control for kirby board */

#include "battery.h"
#include "charger.h"
#include "chipset.h"
#include "console.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "task.h"
#include "timer.h"
#include "tsu6721.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_USBCHARGE, outstr)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

/* TODO XXX: Move this to charger_bq24192.c */
int charger_enable_otg_power(int enabled)
{
	int val, rv;

	rv = i2c_read8(I2C_PORT_HOST, 0xd6, 0x1, &val);
	if (rv)
		return rv;
	val = (val & ~0x3) | (enabled ? 2 : 1);
	return i2c_write8(I2C_PORT_HOST, 0xd6, 0x1, val);
}

int extpower_is_present(void)
{
	return !gpio_get_level(GPIO_AC_PRESENT);
}

static int extpower_set_otg_mode(int enabled)
{
	gpio_set_level(GPIO_BCHGR_OTG, enabled);
	return charger_enable_otg_power(enabled);
}

static void extpower_deferred(void)
{
	int int_val, dev_type, is_otg;
	int ac;
	static int last_ac = -1;

	int_val = tsu6721_get_interrupts();
	dev_type = tsu6721_get_device_type();
	is_otg = dev_type & TSU6721_TYPE_OTG;

	ac = extpower_is_present();
	if (last_ac != ac) {
		last_ac = ac;
		hook_notify(HOOK_AC_CHANGE);
		ccprintf("AC  = %d\n", ac);
	}

	if (!int_val)
		return;

	if (is_otg && gpio_get_level(GPIO_BCHGR_OTG)) {
		extpower_set_otg_mode(1);
		ccprintf("Switch to OTG\n");
	} else if (!is_otg && !gpio_get_level(GPIO_BCHGR_OTG)) {
		extpower_set_otg_mode(0);
		ccprintf("Switch to normal mode\n");
	}
}
DECLARE_DEFERRED(extpower_deferred);

/*****************************************************************************/
/* Hooks */

static void extpower_init(void)
{
	tsu6721_init();
	gpio_enable_interrupt(GPIO_USB_CHG_INT);
	gpio_enable_interrupt(GPIO_AC_PRESENT);
}
DECLARE_HOOK(HOOK_INIT, extpower_init, HOOK_PRIO_LAST);

void extpower_interrupt(enum gpio_signal signal)
{
	hook_call_deferred(extpower_deferred, 0);
}
