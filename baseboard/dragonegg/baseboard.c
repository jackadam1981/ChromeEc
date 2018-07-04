/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* DragonEgg family-specific configuration */
#include "chipset.h"
#include "espi.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "power.h"
#include "util.h"

/******************************************************************************/
/* Wake up pins */
const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_LID_OPEN,
	GPIO_AC_PRESENT,
	GPIO_POWER_BUTTON_L,
};
const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);

/* I2C port map configuration */
/* TODO(b/111125177): Increase these speeds to 400 kHz and verify operation */
const struct i2c_port_t i2c_ports[] = {
	{"eeprom", IT83XX_I2C_CH_A, 100, GPIO_I2C0_SCL, GPIO_I2C0_SDA},
	{"sensor", IT83XX_I2C_CH_B, 100, GPIO_I2C1_SCL, GPIO_I2C1_SDA},
	{"usbc12",  IT83XX_I2C_CH_C, 100, GPIO_I2C2_SCL, GPIO_I2C2_SDA},
	{"usbc0",  IT83XX_I2C_CH_E, 100, GPIO_I2C4_SCL, GPIO_I2C4_SDA},
	{"power",  IT83XX_I2C_CH_F, 100, GPIO_I2C5_SCL, GPIO_I2C5_SDA}
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_SLP_S0_L,
		POWER_SIGNAL_ACTIVE_HIGH | POWER_SIGNAL_DISABLE_AT_BOOT,
		"SLP_S0_DEASSERTED"},
/* TODO(b/111136396): Don't rely on VW signals until they are verified to work
 * properly. CONFIG_HOSTCMD_ESPI_VW_SIGNALS needs to be defined because
 * PCH_PLTRST_L is not routed to the EC. Therefore, for now, switch the #if and
 * #else dependency for this config option.
 */
#ifdef CONFIG_HOSTCMD_ESPI_VW_SIGNALS
	{GPIO_SLP_S3_L,		POWER_SIGNAL_ACTIVE_HIGH, "SLP_S3_DEASSERTED"},
	{GPIO_SLP_S4_L,		POWER_SIGNAL_ACTIVE_HIGH, "SLP_S4_DEASSERTED"},
#else
	{VW_SLP_S3_L,		POWER_SIGNAL_ACTIVE_HIGH, "SLP_S3_DEASSERTED"},
	{VW_SLP_S4_L,		POWER_SIGNAL_ACTIVE_HIGH, "SLP_S4_DEASSERTED"},
#endif
	{GPIO_SLP_SUS_L,	POWER_SIGNAL_ACTIVE_HIGH, "SLP_SUS_DEASSERTED"},
	{GPIO_PG_EC_RSMRST_ODL, POWER_SIGNAL_ACTIVE_HIGH, "RSMRST_L_PGOOD"},
	{GPIO_PG_EC_DSW_PWROK,	POWER_SIGNAL_ACTIVE_HIGH, "DSW_DPWROK"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

/******************************************************************************/
/* Chipset callbacks/hooks */

/* Called on AP S5 -> S3 transition */
static void baseboard_chipset_startup(void)
{
	/* TODD(b/111121615): Need to fill out this hook */
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, baseboard_chipset_startup,
	     HOOK_PRIO_DEFAULT);

/* Called on AP S3 -> S0 transition */
static void baseboard_chipset_resume(void)
{
	/* TODD(b/111121615): Need to fill out this hook */
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, baseboard_chipset_resume, HOOK_PRIO_DEFAULT);

/* Called on AP S0 -> S3 transition */
static void baseboard_chipset_suspend(void)
{
	/* TODD(b/111121615): Need to fill out this hook */
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, baseboard_chipset_suspend,
	     HOOK_PRIO_DEFAULT);

/* Called on AP S3 -> S5 transition */
static void baseboard_chipset_shutdown(void)
{
	/* TODD(b/111121615): Need to fill out this hook */
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, baseboard_chipset_shutdown,
	     HOOK_PRIO_DEFAULT);

void board_hibernate(void)
{
	/*
	 * Disable the TCPC power rail and the PP5000 rail before going into
	 * hibernate. Note, these 2 rails are powered up as the default state in
	 * gpio.inc.
	 */
	/* Disable TCPC power rail */
	gpio_set_level(GPIO_EN_PP3300_TCPC, 0);
	gpio_set_level(GPIO_EN_PP5000, 0);
}
