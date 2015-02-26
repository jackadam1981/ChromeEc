/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* EC for Nuvoton M4 EB configuration */

#include "adc.h"
#include "adc_chip.h"
#include "backlight.h"
#include "chipset.h"
#include "common.h"
#include "driver/temp_sensor/tmp006.h"
#include "extpower.h"
#include "fan.h"
#include "fan_chip.h"
#include "gpio.h"
#include "i2c.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "peci.h"
#include "power.h"
#include "power_button.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "registers.h"
#include "switch.h"
#include "temp_sensor.h"
#include "temp_sensor_chip.h"
#include "timer.h"
#include "thermal.h"
#include "util.h"

#include "gpio_list.h"

/******************************************************************************/
/* ADC channels. Must be in the exactly same order as in enum adc_channel. */
const struct adc_t adc_channels[] = {
	[ADC_CH_0] = {"ADC0", NPCX_ADC_INPUT_CH0, ADC_MAX_VOLT,
			ADC_READ_MAX+1, 0},
	[ADC_CH_1] = {"ADC1", NPCX_ADC_INPUT_CH1, ADC_MAX_VOLT,
			ADC_READ_MAX+1, 0},
	[ADC_CH_2] = {"ADC2", NPCX_ADC_INPUT_CH2, ADC_MAX_VOLT,
			ADC_READ_MAX+1, 0},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/******************************************************************************/
/* PWM channels. Must be in the exactly same order as in enum pwm_channel. */
const struct pwm_t pwm_channels[] = {
	[PWM_CH_FAN] =	{
			.channel = 0,
			/*
			 * flags can reverse the PWM output signal according to
			 * the board design
			 */
			.flags = PWM_CONFIG_ACTIVE_LOW,
			/*
			 * freq_operation = freq_input / prescaler_divider
			 * freq_output = freq_operation / cycle_pulses
			 * and freq_output <= freq_mft
			 */
			.freq = 34,
			/*
			 * cycle_pulses = (cycle_pulses * freq_output) *
			 * RPM_EDGES * RPM_SCALE * 60 / poles / rpm_min
			 */
			.cycle_pulses = 480,
	},
	[PWM_CH_KBLIGHT] = {
			.channel = 1,
			.flags = 0,
			.freq = 10000,
			.cycle_pulses = 100,
			},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

/******************************************************************************/
/* Physical fans. These are logically separate from pwm_channels. */
const struct fan_t fans[] = {
	[FAN_CH_0] = {
		.flags = FAN_USE_RPM_MODE,
		.rpm_min = 1020,
		.rpm_start = 1020,
		.rpm_max = 8190,
		.ch = 0,/* Use PWM/MFT to control fan */
		.pgood_gpio = GPIO_PGOOD_FAN,
		.enable_gpio = -1,
	},
};
BUILD_ASSERT(ARRAY_SIZE(fans) == FAN_CH_COUNT);

/******************************************************************************/
/* MFT channels. These are logically separate from mft_channels. */
const struct mft_t mft_channels[] = {
	[MFT_CH_0] =    {
			.module = NPCX_MFT_MODULE_1,
			.port = NPCX_MFT_MODULE_PORT_TA,
			.default_count = 0xFFFF,
#ifdef CONFIG_MFT_INPUT_LFCLK
			.freq = 32768,
#else
			.freq = 2000000,
#endif
			},
};
BUILD_ASSERT(ARRAY_SIZE(mft_channels) == MFT_CH_COUNT);

/******************************************************************************/
/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"master", I2C_PORT_MASTER, 100,
	 GPIO_MASTER_I2C_SCL, GPIO_MASTER_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/******************************************************************************/
/* Keyboard scan setting */
struct keyboard_scan_config keyscan_config = {
	.output_settle_us = 40,
	.debounce_down_us = 6 * MSEC,
	.debounce_up_us = 30 * MSEC,
	.scan_period_us = 1500,
	.min_post_scan_delay_us = 1000,
	.poll_timeout_us = SECOND,
	.actual_key_mask = {
		0x14, 0xff, 0xff, 0xff, 0xff, 0xf5, 0xff,
		0xa4, 0xff, 0xf6, 0x55, 0xfa, 0xc8  /* full set */
	},
};

#undef ROMLIB
#define ROMLIB (uint32_t)

#include "crc8.h"
uint32_t crc32_hash(uint32_t crc, const void *buf, int size);
extern const uint32_t crc32_tab[];
#include "queue.h"
#include "rsa.h"
extern const uint8_t sha256_tail[];
void mont_mul(const struct rsa_public_key *key,
	      uint32_t *c,
	      const uint32_t *a,
	      const uint32_t *b);
#include "sha1.h"
#include "sha256.h"
extern const uint32_t sha256_h0[8];
extern const uint32_t sha256_k[64];
extern const uint8_t SHA256_digestinfo[];
extern const uint16_t scancode_set1[KEYBOARD_ROWS][KEYBOARD_COLS];
extern const uint16_t scancode_set2[KEYBOARD_ROWS][KEYBOARD_COLS];

extern uint8_t gpio_wui_table[];
extern uint8_t gpio_alt_table[];

// KLUDGE - romlib stuff
uint32_t romlib_table[0x40] __attribute__((section(".romlib.table"))) =
{
	/* crc8.c */
	ROMLIB crc8,
	/* crc.c */
	ROMLIB crc32_tab,
	ROMLIB crc32_hash,
	/* queue.c */
	ROMLIB queue_init,
	ROMLIB queue_is_empty,
	ROMLIB queue_count,
	ROMLIB queue_space,
	ROMLIB queue_add_unit,
	ROMLIB queue_add_units,
	ROMLIB queue_add_memcpy,
	/* static ROMLIB queue_read_safe */
	ROMLIB queue_remove_unit,
	ROMLIB queue_remove_units,
	ROMLIB queue_remove_memcpy,
	ROMLIB queue_peek_units,
	/* rsa.c */
	ROMLIB sha256_tail,
	ROMLIB mont_mul,
	ROMLIB rsa_verify,
	/* sha1.c */
	ROMLIB sha1_update,
	ROMLIB sha1_final,
	ROMLIB sha1_init,
	/* sha256.c */
	ROMLIB sha256_h0,
	ROMLIB sha256_k,
	ROMLIB SHA256_digestinfo,
	ROMLIB SHA256_update,
	ROMLIB SHA256_final,
	ROMLIB SHA256_init,
	/* util.c */
	ROMLIB strlen,
	ROMLIB isspace,
	ROMLIB isdigit,
	ROMLIB isalpha,
	ROMLIB isprint,
	ROMLIB tolower,
	ROMLIB strcasecmp,
	ROMLIB strncasecmp,
	ROMLIB atoi,
	ROMLIB strtoi,
	ROMLIB parse_bool,
	ROMLIB memcmp,
	ROMLIB memcpy,
	ROMLIB memset,
	ROMLIB memmove,
	ROMLIB strzcpy,
	ROMLIB uint64divmod,
	ROMLIB get_next_bit,
	ROMLIB cond_init,
	ROMLIB cond_is,
	ROMLIB cond_set,
	ROMLIB cond_went,
	ROMLIB parse_offset_size,
	/* keyboard_8042.c */
	ROMLIB scancode_set1,
	ROMLIB scancode_set2,
	/* gpio.c */
	ROMLIB gpio_wui_table,
	ROMLIB gpio_alt_table,
};
