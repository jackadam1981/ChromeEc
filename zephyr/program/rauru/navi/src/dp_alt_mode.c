/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "console.h"
#include "gpio.h"
#include "usb_dp_alt_mode.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(karis, LOG_LEVEL_INF);

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ##args)

enum navi_dp_port {
	DP_PORT_NONE = -1,
	DP_PORT_C0 = 0,
	DP_PORT_C1,
	DP_PORT_COUNT,
};

int svdm_get_hpd_gpio(int port)
{
	/* HPD is low active, inverse the result */
	if (port == DP_PORT_C1)
		return !gpio_pin_get_dt(
			GPIO_DT_FROM_NODELABEL(gpio_ec_ap_dp_hpd_l));
	else
		return !gpio_pin_get_dt(
			GPIO_DT_FROM_NODELABEL(gpio_usb_c0_dp_in_hpd));
}

void svdm_set_hpd_gpio(int port, int en)
{
	if (port == DP_PORT_C1)
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_ap_dp_hpd_l),
				!en);
	else
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_usb_c0_dp_in_hpd),
				!en);
}
