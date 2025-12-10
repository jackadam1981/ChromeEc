/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

 #include "gpio/gpio.h"
 #include "gpio/gpio_int.h"
 #include "hooks.h"
 #include "console.h"

 #include <zephyr/devicetree.h>
 #include <zephyr/drivers/gpio.h>
 #include <zephyr/logging/log.h>
 #define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ##args)

 
 void forcepad_disable(void)
 {
	CPRINTS("DBGFP forcepad disable\n");
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_fpad_5v_en),0);
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_fpad_1p8v_en),0);
 }
 
 void forcepad_enable(void)
 {
	CPRINTS("DBGFP forcepad enable\n");
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_fpad_5v_en),1);
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_fpad_1p8v_en),1);
 } 

 void forcepad_interrupt(enum gpio_signal signal)
{
	CPRINTS("DBGFP forcepad interrupt signal %d", signal);
	
	if (gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_fpad_det))) {
		CPRINTS("DBGFP interrupt enable signal %d", signal);
		forcepad_enable();
	} else {
		CPRINTS("DBGFP interrupt disable signal %d", signal);
		forcepad_disable();
	}
}