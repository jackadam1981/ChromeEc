/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB charging control module for Chrome EC */

#include "chipset.h"
#include "common.h"
#include "gpio.h"
#include "system.h"
#include "usb_charge.h"


#ifdef CONFIG_USB_PORT_POWER_SMART_EX
void usb_charge_set_enabled_ex(int port_id, int en)
{
	if (port_id == 0)
		gpio_set_level(GPIO_USB1_ENABLE, en);

	if (port_id == 1)
		gpio_set_level(GPIO_USB2_ENABLE, en);

	if (port_id == 2)
		gpio_set_level(GPIO_USB3_ENABLE, en);

}
#endif
