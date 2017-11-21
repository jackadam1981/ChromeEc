/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel GLK-RVP board-specific configuration */

#include "button.h"
#include "chipset.h"
#include "console.h"
#include "espi.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "ioexpander_pca9555.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "spi.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

#include "gpio_list.h"

#define I2C_PORT_PCA555_BOARD_ID_GPIO	NPCX_I2C_PORT3_0
#define I2C_ADDR_PCA555_BOARD_ID_GPIO	0x40
#define PCA555_BOARD_ID_GPIO_READ(reg, data) \
		pca9555_read(I2C_PORT_PCA555_BOARD_ID_GPIO, \
			I2C_ADDR_PCA555_BOARD_ID_GPIO, (reg), (data))

/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_PCH_SLP_S0_L,
		POWER_SIGNAL_ACTIVE_HIGH | POWER_SIGNAL_DISABLE_AT_BOOT,
		"SLP_S0_DEASSERTED"},
#ifdef CONFIG_ESPI_VW_SIGNALS
	{VW_SLP_S3_L,         POWER_SIGNAL_ACTIVE_HIGH, "SLP_S3_DEASSERTED"},
	{VW_SLP_S4_L,         POWER_SIGNAL_ACTIVE_HIGH, "SLP_S4_DEASSERTED"},
	{VW_SLP_S5_L,         POWER_SIGNAL_ACTIVE_HIGH, "SLP_S4_DEASSERTED"},
#else
	{GPIO_PCH_SLP_S3_L,   POWER_SIGNAL_ACTIVE_HIGH, "SLP_S3_DEASSERTED"},
	{GPIO_PCH_SLP_S4_L,   POWER_SIGNAL_ACTIVE_HIGH, "SLP_S4_DEASSERTED"},
	{GPIO_PCH_SLP_S5_L,   POWER_SIGNAL_ACTIVE_HIGH, "SLP_S5_DEASSERTED"},
#endif /* defined(CONFIG_ESPI_VW_SIGNALS) */
	{GPIO_PCH_SLP_SUS_L,  POWER_SIGNAL_ACTIVE_HIGH, "SLP_SUS_DEASSERTED"},
	{GPIO_RSMRST_L_PGOOD, POWER_SIGNAL_ACTIVE_HIGH, "RSMRST_L_PGOOD"},
	{GPIO_PMIC_DPWROK,    POWER_SIGNAL_ACTIVE_HIGH, "PMIC_DPWROK"},
	{GPIO_ALL_SYS_PWRGD,  POWER_SIGNAL_ACTIVE_HIGH, "ALL_SYS_PWRGD"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"master0",	NPCX_I2C_PORT0_0, 100, GPIO_I2C0_SCL, GPIO_I2C0_SDA},
	{"master1",	NPCX_I2C_PORT1_0, 400, GPIO_I2C1_SCL, GPIO_I2C1_SDA},
	{"master2",	NPCX_I2C_PORT2_0, 100, GPIO_I2C2_SCL, GPIO_I2C2_SDA},
	{"charger",	NPCX_I2C_PORT3_0, 100, GPIO_I2C3_SCL, GPIO_I2C3_SDA},
	{"typec",	NPCX_I2C_PORT7_0, 400, GPIO_I2C7_SCL, GPIO_I2C7_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* Wake-up pins for hibernate */
const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_AC_PRESENT,
	GPIO_LID_OPEN,
	GPIO_POWER_BUTTON_L,
};
const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);

/* Called by APL power state machine when transitioning from G3 to S5 */
static void chipset_pre_init(void)
{
}
DECLARE_HOOK(HOOK_CHIPSET_PRE_INIT, chipset_pre_init, HOOK_PRIO_DEFAULT);


/* Initialize board. */
static void board_init(void)
{
	/* TODO: clear latch set for power button */
	power_button_pch_pulse();
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_FIRST);

/* Called on AP S5 -> S3 transition */
static void board_chipset_startup(void)
{
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, board_chipset_startup, HOOK_PRIO_DEFAULT);

/* Called on AP S3 -> S5 transition */
static void board_chipset_shutdown(void)
{
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, board_chipset_shutdown, HOOK_PRIO_DEFAULT);

void chipset_do_shutdown(void)
{
}

void board_hibernate_late(void)
{
}

void board_hibernate(void)
{
	/*
	 * To support hibernate called from console commands, ectool commands
	 * and key sequence, shutdown the AP before hibernating.
	 */
	chipset_do_shutdown();

	/* Added delay to allow AP to settle down */
	msleep(100);

	/* Power to EC should shut down now */
	while (1)
		;
}

int board_get_version(void)
{
	int data;

	if (PCA555_BOARD_ID_GPIO_READ(PCA9555_CMD_INPUT_PORT_1, &data))
		return -1;

	return data & 0x1F;
}
