/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Board re-init for Rusty board
 * Rusty shares the firmware with Steelix.
 * Steelix is convertible but Rusty is clamshell
 * so some functions should be disabled for clamshell.
 */
#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "cros_cbi.h"
#include "driver/accelgyro_bmi3xx.h"
#include "driver/accelgyro_lsm6dsm.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "motion_sense.h"
#include "motionsense_sensors.h"
#include "tablet_mode.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include "battery.h"
#include "battery_smart.h"
#include "host_command.h"
#include "i2c.h"
#include "timer.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ##args)

LOG_MODULE_REGISTER(board_init, LOG_LEVEL_ERR);

static bool board_is_clamshell;

static void board_setup_init(void)
{
	int ret;
	uint32_t val;

	ret = cros_cbi_get_fw_config(FORM_FACTOR, &val);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG field %d", FORM_FACTOR);
		return;
	}
	if (val == CLAMSHELL) {
		board_is_clamshell = true;
		motion_sensor_count = 0;
		gmr_tablet_switch_disable();
	}
}
DECLARE_HOOK(HOOK_INIT, board_setup_init, HOOK_PRIO_PRE_DEFAULT);

static void disable_base_imu_irq(void)
{
	if (board_is_clamshell) {
		gpio_disable_dt_interrupt(
			GPIO_INT_FROM_NODELABEL(int_base_imu));
		gpio_pin_configure_dt(GPIO_DT_FROM_NODELABEL(base_imu_int_l),
				      GPIO_INPUT | GPIO_PULL_UP);
	}
}
DECLARE_HOOK(HOOK_INIT, disable_base_imu_irq, HOOK_PRIO_POST_DEFAULT);

static bool base_use_alt_sensor;

void motion_interrupt(enum gpio_signal signal)
{
	if (base_use_alt_sensor) {
		lsm6dsm_interrupt(signal);
	} else {
		bmi3xx_interrupt(signal);
	}
}

static void alt_sensor_init(void)
{
	base_use_alt_sensor = cros_cbi_ssfc_check_match(
		CBI_SSFC_VALUE_ID(DT_NODELABEL(base_sensor_1)));

	motion_sensors_check_ssfc();
}
DECLARE_HOOK(HOOK_INIT, alt_sensor_init, HOOK_PRIO_POST_I2C);


static int command_dump_smb_regs(int argc, const char **argv)
{
	int reg;
	int regval;
	int rv;

	for (reg = SB_MANUFACTURER_ACCESS; reg <= SB_MANUFACTURER_DATA; reg++) {
		CPRINTF("[%Xh] = ", reg);
		rv = i2c_read16(I2C_PORT_BATTERY,
				BATTERY_ADDR_FLAGS, reg, &regval);
		if (!rv)
			CPRINTF("0x%04x\n", regval);
		else
			CPRINTF("ERR (%d)\n", rv);
		cflush();
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(dumpbatteryreg, command_dump_smb_regs, NULL, "dump smart battery registers");


static int cmd_sub_cmd1(const struct shell *shell,
				  size_t argc, char **argv)
{
	printk("This is sub cmd 1 of test_shell_command\n");
	return 0;
}

static int cmd_sub_cmd2(const struct shell *shell,
				  size_t argc, char **argv)
{
	printk("This is sub cmd 2 of test_shell_command\n");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_test_shell_command,
	SHELL_CMD(sub_cmd1, NULL, "Sub command 1", cmd_sub_cmd1),
	SHELL_CMD(sub_cmd2, NULL, "Sub command 2", cmd_sub_cmd2),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(test_shell_command, &sub_test_shell_command, "test shell commands", NULL);
