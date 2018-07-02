/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* DragonEgg family-specific configuration */
#include "chipset.h"
#include "espi.h"
#include "gpio.h"
#include "i2c.h"
#include "power.h"
#include "util.h"

/******************************************************************************/
/* I2C port map configuration */
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
#ifdef CONFIG_HOSTCMD_ESPI_VW_SIGNALS
	{VW_SLP_S3_L,	      POWER_SIGNAL_ACTIVE_HIGH, "SLP_S3_DEASSERTED"},
	{VW_SLP_S4_L,	      POWER_SIGNAL_ACTIVE_HIGH, "SLP_S4_DEASSERTED"},
#else
	{GPIO_SLP_S3_L,   POWER_SIGNAL_ACTIVE_HIGH, "SLP_S3_DEASSERTED"},
	{GPIO_SLP_S4_L,   POWER_SIGNAL_ACTIVE_HIGH, "SLP_S4_DEASSERTED"},
#endif
	{GPIO_SLP_SUS_L,  POWER_SIGNAL_ACTIVE_HIGH, "SLP_SUS_DEASSERTED"},
	{GPIO_RSMRST_L_PGOOD, POWER_SIGNAL_ACTIVE_HIGH, "RSMRST_L_PGOOD"},
	{GPIO_PG_EC_DSW_PWROK,    POWER_SIGNAL_ACTIVE_HIGH, "DSW_DPWROK"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);
