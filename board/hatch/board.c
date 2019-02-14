/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Hatch board-specific configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "driver/ppc/sn5s330.h"
#include "ec_commands.h"
#include "extpower.h"
#include "fan.h"
#include "fan_chip.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "spi.h"
#include "switch.h"
#include "system.h"
#include "temp_sensor.h"
#include "thermal.h"
#include "thermistor.h"
#include "uart.h"
#include "usb_pd.h"
#include "usbc_ppc.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static void ppc_interrupt(enum gpio_signal signal)
{
	switch (signal) {
	case GPIO_USB_C0_PPC_INT_ODL:
		sn5s330_interrupt(0);
		break;

	case GPIO_USB_C1_PPC_INT_ODL:
		sn5s330_interrupt(1);
		break;

	default:
		break;
	}
}

static void tcpc_alert_event(enum gpio_signal signal)
{
	int port = -1;

	switch (signal) {
	case GPIO_USB_C0_TCPC_INT_ODL:
		port = 0;
		break;
	case GPIO_USB_C1_TCPC_INT_ODL:
		port = 1;
		break;
	default:
		return;
	}

	schedule_deferred_pd_interrupt(port);
}

static void hdmi_hpd_interrupt(enum gpio_signal signal)
{
	baseboard_mst_enable_control(MST_HDMI, gpio_get_level(signal));
}

#include "gpio_list.h" /* Must come after other header files. */

/******************************************************************************/
/* SPI devices */
const struct spi_device_t spi_devices[] = {
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

/******************************************************************************/
/* PWM channels. Must be in the exactly same order as in enum pwm_channel. */
const struct pwm_t pwm_channels[] = {
	[PWM_CH_KBLIGHT]   = { .channel = 3, .flags = 0, .freq = 10000 },
	[PWM_CH_FAN] = {.channel = 5, .flags = PWM_CONFIG_OPEN_DRAIN,
			.freq = 25000},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

/******************************************************************************/
/* Physical fans. These are logically separate from pwm_channels. */

const struct fan_conf fan_conf_0 = {
	.flags = FAN_USE_RPM_MODE,
	.ch = MFT_CH_0,	/* Use MFT id to control fan */
	.pgood_gpio = -1,
	.enable_gpio = GPIO_EN_PP5000_FAN,
};

/* Default */
const struct fan_rpm fan_rpm_0 = {
	.rpm_min = 3100,
	.rpm_start = 3100,
	.rpm_max = 6900,
};

struct fan_t fans[FAN_CH_COUNT] = {
	[FAN_CH_0] = { .conf = &fan_conf_0, .rpm = &fan_rpm_0, },
};

/******************************************************************************/
/* MFT channels. These are logically separate from pwm_channels. */
const struct mft_t mft_channels[] = {
	[MFT_CH_0] = {NPCX_MFT_MODULE_1, TCKC_LFCLK, PWM_CH_FAN},
};
BUILD_ASSERT(ARRAY_SIZE(mft_channels) == MFT_CH_COUNT);

/* ADC channels */
const struct adc_t adc_channels[] = {
	[ADC_TEMP_SENSOR_1] = {
		"TEMP_AMB", NPCX_ADC_CH0, ADC_MAX_VOLT, ADC_READ_MAX+1, 0},
	[ADC_TEMP_SENSOR_2] = {
		"TEMP_CHARGER", NPCX_ADC_CH1, ADC_MAX_VOLT, ADC_READ_MAX+1, 0},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

const struct temp_sensor_t temp_sensors[] = {
	[TEMP_SENSOR_1] = {.name = "Temp1",
				 .type = TEMP_SENSOR_TYPE_BOARD,
				 .read = get_temp_3v3_51k1_47k_4050b,
				 .idx = ADC_TEMP_SENSOR_1,
				 .action_delay_sec = 1},
	[TEMP_SENSOR_2] = {.name = "Temp2",
				 .type = TEMP_SENSOR_TYPE_BOARD,
				 .read = get_temp_3v3_51k1_47k_4050b,
				 .idx = ADC_TEMP_SENSOR_2,
				 .action_delay_sec = 1},
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);


/* Nami/Vayne Remote 1, 2 */
const static struct ec_thermal_config thermal_a = {
	.temp_host = {
		[EC_TEMP_THRESH_WARN] = 0,
		[EC_TEMP_THRESH_HIGH] = C_TO_K(75),
		[EC_TEMP_THRESH_HALT] = C_TO_K(80),
	},
	.temp_host_release = {
		[EC_TEMP_THRESH_WARN] = 0,
		[EC_TEMP_THRESH_HIGH] = C_TO_K(65),
		[EC_TEMP_THRESH_HALT] = 0,
	},
	.temp_fan_off = C_TO_K(39),
	.temp_fan_max = C_TO_K(50),
};

struct ec_thermal_config thermal_params[TEMP_SENSOR_COUNT];

static void setup_fans(void)
{
	thermal_params[TEMP_SENSOR_1] = thermal_a;
	thermal_params[TEMP_SENSOR_2] = thermal_a;
}
DECLARE_HOOK(HOOK_INIT, setup_fans, HOOK_PRIO_DEFAULT);

void board_overcurrent_event(int port, int is_overcurrented)
{
	/* Sanity check the port. */
	if ((port < 0) || (port >= CONFIG_USB_PD_PORT_COUNT))
		return;

	/* Note that the level is inverted because the pin is active low. */
	gpio_set_level(GPIO_USB_C_OC_ODL, !is_overcurrented);
}

/* MPS Programming */
struct mp2979_reg_info {
	int page;
	int cmd;
	int value;
};

static const struct mp2979_reg_info mp2979_reg[] = {
	{0, 0xCE, 0x720A},
	{0, 0x2B, 0x1C02},
	{0, 0xE4, 0xAC20},
	{0, 0xCF, 0x00FA},
	{0, 0xF1, 0x0005},
	{0, 0x2F, 0xB8A6},
	{1, 0xE4, 0x0610},
	{1, 0x2F, 0x0000},
	{2, 0xE4, 0x0308},
	{2, 0x2F, 0x0000},
};

int mp2979_check_registers(void)
{
	static int verified;
	int i;
	int rv;
	int value;
	int change_made = 0;
	int page;
	int pe_level;

	/* If verifed is set, then this check already has been done */
	if (verified) {
		CPRINTS("MP2979: Registers were already verfied!");
		return EC_SUCCESS;
	}

	pe_level = gpio_get_level(GPIO_MP2979_PE);
	/* Enable Program Engine */
	gpio_set_flags(GPIO_MP2979_PE, GPIO_OUT_HIGH);

	CPRINTS("MP2979: PE pin: before %d after %d",
		pe_level, gpio_get_level(GPIO_MP2979_PE));

	/* Check register values and correct if necessary */
	for (i = 0; i < ARRAY_SIZE(mp2979_reg); i++) {
		/* Ensure on correct page */
		rv = i2c_read8(BOARD_MP2979_PORT, BOARD_MP2979_ADDR,
			       BOARD_MP2979_PAGE_CMD, &page);

		CPRINTS("MP2979[%d]: page read = %d, rv = %d", i, page, rv);
		if (rv)
			goto mp2979_exit;
		if (page != mp2979_reg[i].page) {
			rv = i2c_write8(BOARD_MP2979_PORT, BOARD_MP2979_ADDR,
			       BOARD_MP2979_PAGE_CMD, mp2979_reg[i].page);
			rv = i2c_read8(BOARD_MP2979_PORT, BOARD_MP2979_ADDR,
			       BOARD_MP2979_PAGE_CMD, &page);
			if (rv || (page != mp2979_reg[i].page)) {
				CPRINTS("mp2979: Failed page set!");
				goto mp2979_exit;
			}
		}
		/* Get current value of register */
		rv = i2c_read16(BOARD_MP2979_PORT, BOARD_MP2979_ADDR,
			       mp2979_reg[i].cmd, &value);
		if (rv)
			goto mp2979_exit;

		CPRINTS("MP2979[%d]: page %d off 0x%x rd = 0x%x, des = 0x%x",
			i, page, mp2979_reg[i].cmd, value, mp2979_reg[i].value);
		if (value != mp2979_reg[i].value) {
			rv = i2c_write16(BOARD_MP2979_PORT, BOARD_MP2979_ADDR,
			       mp2979_reg[i].cmd, mp2979_reg[i].value);
			if (rv)
				goto mp2979_exit;
			/* count number of registers changed */
			change_made++;
		}
	}

	/* If any registers have changed then need to write to flash */
	if (change_made) {
		uint8_t buf = 0x15;

		CPRINTS("MP2979: %d registers were changed", change_made);
		/* Store all user data by write cmd = 0x15 with no data */
		rv = i2c_xfer(BOARD_MP2979_PORT, BOARD_MP2979_ADDR, &buf,
			      1, 0, 0);
		if (rv)
			goto mp2979_exit;
		msleep(300);
	}

	/*
	 * At this point, all registers were either correct, or have been
	 * updated to the desired values.
	 */
	verified = 1;
	rv = EC_SUCCESS;

mp2979_exit:
	gpio_set_flags(GPIO_MP2979_PE, GPIO_INPUT);
	return rv;
}
