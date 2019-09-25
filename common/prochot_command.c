/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "i2c.h"
#include "isl923x.h"
#include "pmic_mp2949.h"
#include "pmic_bd99992gw.h"

#ifdef I2C_ADDR_BD99992_FLAGS
static void dump_bd99992(void)
{
	int i8;

	ccprintf("IMVP8:\n");
	ccprintf("\n");

	if (i2c_read8(I2C_PORT_PMIC, I2C_ADDR_BD99992_FLAGS,
		      BD99992GW_REG_IADPLMTCRT, &i8) != EC_SUCCESS)
		return;
	ccprintf("IADPLMTCRT 0x%02x (%s)\n", i8,
		 (i8 & BIT(6)) ? "enabled" : "disabled");

	if (i2c_read8(I2C_PORT_PMIC, I2C_ADDR_BD99992_FLAGS,
		      BD99992GW_REG_VDLMTCRT, &i8) != EC_SUCCESS)
		return;
	ccprintf("VDLMTCRT 0x%02x (%s)\n", i8,
		 (i8 & BIT(6)) ? "enabled" : "disabled");

	if (i2c_read8(I2C_PORT_PMIC, I2C_ADDR_BD99992_FLAGS,
		      BD99992GW_REG_IBATTLMTCRT, &i8) != EC_SUCCESS)
		return;
	ccprintf("IBATTLMTCRT 0x%02x (%s)\n", i8,
		 (i8 & BIT(6)) ? "enabled" : "disabled");

	if (i2c_read8(I2C_PORT_PMIC, I2C_ADDR_BD99992_FLAGS,
		      BD99992GW_REG_PMUINT, &i8) != EC_SUCCESS)
		return;
	ccprintf("PMUINT 0x%02x\n", i8);

	if (i2c_read8(I2C_PORT_PMIC, I2C_ADDR_BD99992_FLAGS,
		      BD99992GW_REG_PWRSRCINT, &i8) != EC_SUCCESS)
		return;
	ccprintf("PWRSRCINT 0x%02x (%s)\n", i8,
		 (i8 & BIT(3)) ? "PMICHOT" : "OK");

	ccprintf("\n");
}
#endif /* I2C_ADDR_BD99992_FLAGS */

#ifdef I2C_ADDR_MP2949_FLAGS
static void dump_mp2949_ph(void)
{
	int i8;
	int i16;
	int temp;
	int thr;

	ccprintf("PMIC:\n");

	i8 = 0;
	if (i2c_write8(I2C_PORT_PMIC, I2C_ADDR_MP2949_FLAGS,
		      MP2949_PAGE, i8) != EC_SUCCESS)
		goto bail;

	if (i2c_read16(I2C_PORT_PMIC, I2C_ADDR_MP2949_FLAGS,
		     MP2949_LAST_FAULT_BLOCK, &i16) != EC_SUCCESS)
		goto bail;
	ccprintf("LAST_FAULT_BLOCK 0x%04x\n", i16);

	if (i2c_read8(I2C_PORT_PMIC, I2C_ADDR_MP2949_FLAGS,
		     MP2949_MFR_TEMP_MAX, &thr) != EC_SUCCESS)
		goto bail;
	ccprintf("Max temp threshold %uC\n", thr);

	if (i2c_read16(I2C_PORT_PMIC, I2C_ADDR_MP2949_FLAGS,
		       MP2949_READ_TEMPERATURE, &temp) != EC_SUCCESS)
		goto bail;
	ccprintf("READ_TEMPERATURE %uC (%s)\n", temp,
		 (temp > thr) ? "tripped" : "OK");

bail:
	ccprintf("\n");
}
#endif /* I2C_PORT_PMIC */

#ifdef CONFIG_CHARGER_ISL9238
static void dump_isl9238_ph(void)
{
	static const char * const ph_debounce[] = {
		"7us",
		"100us",
		"500us",
		"1ms",
	};
	static const char * const ph_duration[] = {
		"10ms",
		"20ms",
		"15ms",
		"5ms",
		"1ms",
		"500us",
		"100us",
		"0s",
	};
	int i16;
	int vs_trip;
	int dc_trip;
	int ac_trip;

	ccprintf("ISL9238:\n");

	if (i2c_read16(I2C_PORT_CHARGER, ISL923X_ADDR_FLAGS,
		       ISL923X_REG_INFO1, &i16) != EC_SUCCESS)
		return;
	vs_trip = (i16 & BIT(10)) != 0;
	dc_trip = (i16 & BIT(11)) != 0;
	ac_trip = (i16 & BIT(12)) != 0;

	if (i2c_read16(I2C_PORT_POWER, ISL923X_ADDR_FLAGS,
		       ISL923X_REG_PROCHOT_AC, &i16) != EC_SUCCESS)
		return;
	i16 &= ISL923X_REG_PROCHOT_AC_MASK;
	ccprintf("ACProchot# threshold %umA (%s)\n",
		 i16, ac_trip ? "tripped" : "OK");

	if (i2c_read16(I2C_PORT_POWER, ISL923X_ADDR_FLAGS,
		       ISL923X_REG_PROCHOT_DC, &i16) != EC_SUCCESS)
		return;
	i16 &= ISL923X_REG_PROCHOT_DC_MASK;
	ccprintf("DCProchot# threshold %umA (%s)\n",
		 i16, dc_trip ? "tripped" : "OK");

	if (i2c_read16(I2C_PORT_POWER, ISL923X_ADDR_FLAGS,
		       ISL923X_REG_CONTROL1, &i16) != EC_SUCCESS)
		return;
	ccprintf("Low_VSYS_Prochot# reference 6.%uV (%s)\n", (i16 & 0x03) * 3,
		 vs_trip ? "tripped" : "OK");

	if (i2c_read16(I2C_PORT_POWER, ISL923X_ADDR_FLAGS,
		       ISL923X_REG_CONTROL2, &i16) != EC_SUCCESS)
		return;
	ccprintf("xxProchot# debounce %s\n", ph_debounce[(i16 >> 9) & 0x03]);
	ccprintf("xxProchot# duration %s\n", ph_duration[(i16 >> 6) & 0x07]);

	ccprintf("\n");
}
#endif /* CONFIG_CHARGER_ISL9238 */

static int command_prochot(int argc, char **argv)
{
	int level;

	if (argc > 1)
		return EC_ERROR_PARAM_COUNT;

#ifdef GPIO_CPU_PROCHOT
	ccprintf("EC:\n");
	level = gpio_get_level(GPIO_CPU_PROCHOT);
	ccprintf("GPIO_CPU_PROCHOT %u (%s)\n", level,
		 (level ^ !IS_ENABLED(CONFIG_CPU_PROCHOT_ACTIVE_LOW)) ?
		 "OK" : "tripped");
	ccprintf("\n");
#endif /* GPIO_CPU_PROCHOT */

	if (IS_ENABLED(CONFIG_CHARGER_ISL9238))
		dump_isl9238_ph();

#ifdef I2C_ADDR_MP2949_FLAGS
	dump_mp2949_ph();
#endif /* I2C_ADDR_MP2949_FLAGS */

	dump_bd99992();

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND
	(prochot, command_prochot,
	 "",
	 "Dump PROCHOT sources");
