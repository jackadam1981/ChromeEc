/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "cros_board_info.h"
#include "cros_cbi.h"
#include "driver/accel_bma4xx.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "motionsense_sensors.h"
#include "tablet_mode.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(board_sensor, LOG_LEVEL_INF);

void lid_accel_interrupt(enum gpio_signal signal)
{
	bma4xx_interrupt(signal);
}

static void gmr_init(void)
{
	int ec_fwconfig;
	int ret = cros_cbi_get_fw_config(FORM_FACTOR, &ec_fwconfig);

	if (ret < 0) {
		LOG_ERR("error retriving CBI config: %d", ret);
		return;
	}

	if (ec_fwconfig == CLAMSHELL) {
		gpio_disable_dt_interrupt(
			GPIO_INT_FROM_NODELABEL(int_lid_accel));
		gpio_pin_configure_dt(GPIO_DT_FROM_NODELABEL(gpio_imu_int_ec_l),
				      GPIO_INPUT | GPIO_PULL_UP);
		gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_imu));
		gpio_pin_configure_dt(
			GPIO_DT_FROM_NODELABEL(gpio_lid_accel_int_ec_l),
			GPIO_INPUT | GPIO_PULL_UP);
		gmr_tablet_switch_disable();
		LOG_INF("Board is Clamshell");

	} else if (ec_fwconfig == CONVERTIBLE) {
		LOG_INF("Board is Convertible");
	}
}
DECLARE_HOOK(HOOK_INIT, gmr_init, HOOK_PRIO_DEFAULT);
