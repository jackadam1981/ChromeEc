/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* STM32G431 Nucleo-64 board-specific configuration */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "uart.h"
#include "util.h"

#include "gpio_list.h" /* Must come after other header files. */

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

#define I2C_TEST_PORT0 0

struct power_seq {
	enum gpio_signal signal;
	int pol;
	uint32_t delay;
};

const struct power_seq board_power_seq[] = {
	{GPIO_EN_AC_JACK,               1, 20},
	{GPIO_EN_PP5000_A,              1, 31},
	{GPIO_EN_PP3300_BB,             1, 100},
	{GPIO_EN_BB,                    1, 30},
	{GPIO_EN_PP1100_A,              1, 30},
	{GPIO_EN_PP1050_A,              1, 30},
	{GPIO_EN_PP1200_A,              1, 20},
	{GPIO_EN_PP5000_C,              1, 20},
	{GPIO_EN_PP5000_HSPORT,         1, 31},
	{GPIO_EN_DP_SINK,               1, 80},
	{GPIO_MST_RST_L,                1, 20},
	{GPIO_MST_LP_CTL_L,             1, 41},
	{GPIO_EC_HUB2_RESET_L,          1, 41},
	{GPIO_EC_HUB3_RESET_L,          1, 33},
	{GPIO_DP_SINK_RESET,            1, 100},
	{GPIO_USBC_DP_PD_RST_L,         1, 100},
	{GPIO_USBC_UF_RESET_L,          1, 33},
	{GPIO_DEMUX_DUAL_DP_PD_N,       1, 100},
	{GPIO_DEMUX_DUAL_DP_RESET_N,    1, 100},
	{GPIO_DEMUX_DP_HDMI_PD_N,       1, 10},
	{GPIO_DEMUX_DUAL_DP_MODE,       1, 10},
	{GPIO_DEMUX_DP_HDMI_MODE,       1, 1},
};

static int board_power_sequence(void)
{
	int i;

	for(i = 0; i < ARRAY_SIZE(board_power_seq); i++) {
		gpio_set_level(board_power_seq[i].signal,
			       board_power_seq[i].pol);
		msleep(board_power_seq[i].pol);
	}

	return EC_SUCCESS;
}

static void board_init(void)
{
	/* Turn on power rails */
	board_power_sequence();
	CPRINTS("board: Power rails enabled");
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

void test_i2c_access(void)
{
	int rv;
	int i2c_reg;

	/* Read register address 0 of PS8805 */
	rv = i2c_read16(I2C_PORT_USBC, 0x36, 0, &i2c_reg);
	if (rv)
		CPRINTS("ps8805 read failed you dummy!!!!!!!!!! Error = %d",
			rv);
	else
		CPRINTS("ps8805 reg 0 = 0x%x", i2c_reg);
}

static void led_second(void)
{
#if I2C_TEST_PORT0
	static int count;
	static int attempt;

	if ((count++ & 2) && (attempt < 3)) {
		test_i2c_access();
		attempt++;
	}
#endif
}
DECLARE_HOOK(HOOK_SECOND, led_second, HOOK_PRIO_DEFAULT);

static int board_read_eeprom(uint8_t offset, uint8_t *in, int in_size)
{
	return i2c_read_block(I2C_PORT_EEPROM, I2C_ADDR_EEPROM_FLAGS,
			      offset, in, in_size);
}

/*
 * Get board information from EEPROM
 */
int read_board_info(void)
{
	uint8_t eeprom_data[4];

	CPRINTS("Reading board info");

	/* Read header */
	if (board_read_eeprom(0, eeprom_data, sizeof(eeprom_data))) {
		CPRINTS("Failed to read header");
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

#define PS8822_ADDR (0xC0 / 2)
int read_usbc_reg(void)
{
	int reg_0;
	int offset = 1;
	int port = I2C_PORT_USBC;
	int rv;
	int i;

	for (i = 0; i < 1; i++) {
		reg_0 = 0;
		rv = i2c_write8(I2C_PORT_USBC, PS8822_ADDR, offset, 0x55);
		if (rv)
			CPRINTS("i2c[%d]: access failed! rv = %d", port, rv);
		else
			CPRINTS("i2c[%d]: ps8822 register 0x%x = 0x%x", port,
				offset, reg_0);

	}

	return 0;
}

void eeprom_test_write(void)
{
	int rv;

	rv = i2c_write8(I2C_PORT_EEPROM, I2C_ADDR_EEPROM_FLAGS, 0x0A, 0x55);
	if (rv)
		CPRINTS("you fucked it up again, you fucked up Scott!");
	else
		CPRINTS("you are a fucking rock star Scott!");
}

static int console_qsi(int argc, char **argv)
{
	int i;

	for (i = 0; i < 1; i++) {
		//read_board_info();
		//read_usbc_reg();
		eeprom_test_write();
		CPRINTS("**************************************************");
	}

	return 0;
}
DECLARE_CONSOLE_COMMAND(qsi, console_qsi,
			"<nothing>",
			"start qsi test");
