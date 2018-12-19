/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Hatch family-specific configuration */
#include "chipset.h"
#include "console.h"
#include "espi.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "power.h"
#include "tcpci.h"
#include "timer.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)


/******************************************************************************/
/* I2C port map configuration */
const struct i2c_port_t i2c_ports[] = {
	{"sensor",  I2C_PORT_SENSOR,  100, GPIO_I2C0_SCL, GPIO_I2C0_SDA},
	{"ppc0",    I2C_PORT_PPC0,    100, GPIO_I2C1_SCL, GPIO_I2C1_SDA},
	{"tcpc1",   I2C_PORT_TCPC1,   100, GPIO_I2C2_SCL, GPIO_I2C2_SDA},
	{"tcpc0",   I2C_PORT_TCPC0,   100, GPIO_I2C3_SCL, GPIO_I2C3_SDA},
	{"power",   I2C_PORT_POWER,   100, GPIO_I2C5_SCL, GPIO_I2C5_SDA},
	{"eeprom",  I2C_PORT_EEPROM,  100, GPIO_I2C7_SCL, GPIO_I2C7_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* power signal list. */
const struct power_signal_info power_signal_list[] = {

	[X86_SLP_S0_DEASSERTED] = {GPIO_SLP_S0_L,
		POWER_SIGNAL_ACTIVE_HIGH | POWER_SIGNAL_DISABLE_AT_BOOT,
		"SLP_S0_DEASSERTED"},
#ifdef CONFIG_HOSTCMD_ESPI_VW_SIGNALS
	[X86_SLP_S3_DEASSERTED] = {VW_SLP_S3_L, POWER_SIGNAL_ACTIVE_HIGH,
				   "SLP_S3_DEASSERTED"},
	[X86_SLP_S4_DEASSERTED] = {VW_SLP_S4_L, POWER_SIGNAL_ACTIVE_HIGH,
				   "SLP_S4_DEASSERTED"},
#else
	[X86_SLP_S3_DEASSERTED] = {GPIO_SLP_S3_L, POWER_SIGNAL_ACTIVE_HIGH,
				   "SLP_S3_DEASSERTED"},
	[X86_SLP_S4_DEASSERTED] = {GPIO_SLP_S4_L, POWER_SIGNAL_ACTIVE_HIGH,
				   "SLP_S4_DEASSERTED"},
#endif
	[X86_RSMRST_L_PGOOD] = {GPIO_PG_EC_RSMRST_L, POWER_SIGNAL_ACTIVE_HIGH,
				"RSMRST_L_PGOOD"},
	[PP5000_A_PGOOD] = {GPIO_PP5000_A_PG_OD, POWER_SIGNAL_ACTIVE_HIGH,
			    "PP5000_A_PGOOD"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

/******************************************************************************/
/* Chipset callbacks/hooks */

/* Called by APL power state machine when transitioning from G3 to S5 */
void chipset_pre_init_callback(void)
{
	/*
	 * TODO (b/122265772): Need to use CONFIG_POWER_PP5000_CONTROL, but want
	 * to do that after some refactoring so that more than 1 signal can be
	 * tracked if necessary.
	 */

	/* Enable 5.0V and 3.3V rails, and wait for Power Good */
	/* Turn on PP5000_A rail */
	gpio_set_level(GPIO_EN_PP5000_A, 1);
	/* Turn on A (except PP5000_A) rails*/
	gpio_set_level(GPIO_EN_A_RAILS, 1);

	/* Ensure that PP5000_A rail is stable */
	while (!gpio_get_level(GPIO_PP5000_A_PG_OD))
		;

}

/* Called on AP S5 -> S3 transition */
static void baseboard_chipset_startup(void)
{
	/* TODD(b/122266850): Need to fill out this hook */
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, baseboard_chipset_startup,
	     HOOK_PRIO_DEFAULT);

/* Called on AP S0iX -> S0 transition */
static void baseboard_chipset_resume(void)
{
	/* TODD(b/122266850): Need to fill out this hook */
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, baseboard_chipset_resume, HOOK_PRIO_DEFAULT);

/* Called on AP S0 -> S0iX transition */
static void baseboard_chipset_suspend(void)
{
	/* TODD(b/122266850): Need to fill out this hook */
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, baseboard_chipset_suspend,
	     HOOK_PRIO_DEFAULT);

/* Called on AP S3 -> S5 transition */
static void baseboard_chipset_shutdown(void)
{
	/* TODD(b/122266850): Need to fill out this hook */
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, baseboard_chipset_shutdown,
	     HOOK_PRIO_DEFAULT);
