/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel-RVP family-specific configuration */

#include "adc_chip.h"
#include "charge_state.h"
#include "espi.h"
#include "fan.h"
#include "hooks.h"
#include "ioexpander_pca9555.h"
#include "keyboard_scan.h"
#include "peci.h"
#include "power.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "temp_sensor.h"
#include "thermistor.h"

/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	[X86_SLP_S0_DEASSERTED] = {
		GPIO_PCH_SLP_S0_L,
		POWER_SIGNAL_ACTIVE_HIGH | POWER_SIGNAL_DISABLE_AT_BOOT,
		"SLP_S0_DEASSERTED",
	},
#ifdef CONFIG_HOSTCMD_ESPI_VW_SLP_SIGNALS
	[X86_SLP_S3_DEASSERTED] = {
		VW_SLP_S3_L,
		POWER_SIGNAL_ACTIVE_HIGH,
		"SLP_S3_DEASSERTED",
	},
	[X86_SLP_S4_DEASSERTED] = {
		VW_SLP_S4_L,
		POWER_SIGNAL_ACTIVE_HIGH,
		"SLP_S4_DEASSERTED",
	},
#else
	[X86_SLP_S3_DEASSERTED] = {
		GPIO_PCH_SLP_S3_L,
		POWER_SIGNAL_ACTIVE_HIGH,
		"SLP_S3_DEASSERTED",
	},
	[X86_SLP_S4_DEASSERTED] = {
		GPIO_PCH_SLP_S4_L,
		POWER_SIGNAL_ACTIVE_HIGH,
		"SLP_S4_DEASSERTED",
	},
#endif
	[X86_RSMRST_L_PGOOD] = {
		GPIO_RSMRST_L_PGOOD,
		POWER_SIGNAL_ACTIVE_HIGH,
		"RSMRST_L_PGOOD",
	},
	[X86_ALL_SYS_PWRGD] = {
		GPIO_ALL_SYS_PWRGD,
		POWER_SIGNAL_ACTIVE_HIGH,
		"ALL_SYS_PWRGD",
	},
#if defined(CONFIG_CHIPSET_ICELAKE)
	[X86_SLP_SUS_DEASSERTED] = {
		GPIO_PCH_SLP_SUS_L,
		POWER_SIGNAL_ACTIVE_HIGH,
		"SLP_SUS_DEASSERTED",
	},
	[X86_DSW_DPWROK] = {
		GPIO_DSW_DPWROK,
		POWER_SIGNAL_ACTIVE_HIGH,
		"DSW_DPWROK",
	},
#elif defined(CONFIG_CHIPSET_COMETLAKE)
	[PP5000_A_PGOOD] = {
		GPIO_PP5000_A_PG_OD,
		POWER_SIGNAL_ACTIVE_HIGH,
		"PP5000_A_PGOOD",
	},
#endif
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

/* Wake-up pins for hibernate */
const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_AC_PRESENT,
	GPIO_LID_OPEN,
	GPIO_POWER_BUTTON_L,
};
const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);

/* Keyboard scan setting */
struct keyboard_scan_config keyscan_config = {
	.output_settle_us = 35,
	.debounce_down_us = 5 * MSEC,
	.debounce_up_us = 40 * MSEC,
	.scan_period_us = 3 * MSEC,
	.min_post_scan_delay_us = 1000,
	.poll_timeout_us = 100 * MSEC,
	.actual_key_mask = {
		0x14, 0xff, 0xff, 0xff, 0xff, 0xf5, 0xff,
		0xa4, 0xff, 0xfe, 0x55, 0xfa, 0xca  /* full set */
	},
};

/* ADC channels */
const struct adc_t adc_channels[] = {
	[ADC_TEMP_SNS_AMBIENT] = {
		.name = "ADC_TEMP_SNS_AMBIENT",
		.factor_mul = ADC_MAX_MVOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
		.channel = ADC_TEMP_SNS_AMBIENT_CHANNEL,
	},
	[ADC_TEMP_SNS_DDR] = {
		.name = "ADC_TEMP_SNS_DDR",
		.factor_mul = ADC_MAX_MVOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
		.channel = ADC_TEMP_SNS_DDR_CHANNEL,
	},
	[ADC_TEMP_SNS_SKIN] = {
		.name = "ADC_TEMP_SNS_SKIN",
		.factor_mul = ADC_MAX_MVOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
		.channel = ADC_TEMP_SNS_SKIN_CHANNEL,
	},
	[ADC_TEMP_SNS_VR] = {
		.name = "ADC_TEMP_SNS_VR",
		.factor_mul = ADC_MAX_MVOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
		.channel = ADC_TEMP_SNS_VR_CHANNEL,
	},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

#ifdef CONFIG_TEMP_SENSOR
/* Temperature sensors */
const struct temp_sensor_t temp_sensors[] = {
	[TEMP_SNS_AMBIENT] = {
		.name = "Ambient",
		.type = TEMP_SENSOR_TYPE_BOARD,
		.read = get_temp_3v0_22k6_47k_4050b,
		.idx = ADC_TEMP_SNS_AMBIENT,
		.action_delay_sec = 5,
	},
	[TEMP_SNS_BATTERY] = {
		.name = "Battery",
		.type = TEMP_SENSOR_TYPE_BATTERY,
		.read = charge_get_battery_temp,
		.idx = 0,
		.action_delay_sec = 1,
	},
	[TEMP_SNS_DDR] = {
		.name = "DDR",
		.type = TEMP_SENSOR_TYPE_BOARD,
		.read = get_temp_3v0_22k6_47k_4050b,
		.idx = ADC_TEMP_SNS_DDR,
		.action_delay_sec = 1,
	},
#ifdef CONFIG_PECI
	[TEMP_SNS_PECI] = {
		.name = "PECI",
		.type = TEMP_SENSOR_TYPE_CPU,
		.read = peci_temp_sensor_get_val,
		.idx = 0,
		.action_delay_sec = 1,
	},
#endif
	[TEMP_SNS_SKIN] = {
		.name = "Skin",
		.type = TEMP_SENSOR_TYPE_BOARD,
		.read = get_temp_3v0_22k6_47k_4050b,
		.idx = ADC_TEMP_SNS_SKIN,
		.action_delay_sec = 1,
	},
	[TEMP_SNS_VR] = {
		.name = "VR",
		.type = TEMP_SENSOR_TYPE_BOARD,
		.read = get_temp_3v0_22k6_47k_4050b,
		.idx = ADC_TEMP_SNS_VR,
		.action_delay_sec = 1,
	},
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);
#endif

/* PWM channels */
const struct pwm_t pwm_channels[] = {
	[PWM_CH_FAN] = {
		.channel = PWM_HW_CH_DCR2,
		.flags = PWM_CONFIG_ACTIVE_LOW,
		.freq_hz = 30000,
		.pcfsr_sel = PWM_PRESCALER_C4,
	},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

#ifdef CONFIG_FANS
/* Physical fan config */
const struct fan_conf fan_conf_0 = {
	.flags = FAN_USE_RPM_MODE,
	.ch = 0,
	.pgood_gpio = GPIO_ALL_SYS_PWRGD,
	.enable_gpio = GPIO_FAN_POWER_EN,
};

/* Physical fan rpm config */
const struct fan_rpm fan_rpm_0 = {
	.rpm_min = 3100,
	.rpm_start = 3100,
	.rpm_max = 6900,
};

/* FAN channels */
struct fan_t fans[] = {
	[FAN_CH_0] = {
		.conf = &fan_conf_0,
		.rpm = &fan_rpm_0,
	},
};
BUILD_ASSERT(ARRAY_SIZE(fans) == FAN_CH_COUNT);

#ifdef CHIP_FAMILY_IT83XX
/*
 * PWM HW channelx binding tachometer channelx for fan control.
 * Four tachometer input pins but two tachometer modules only,
 * so always binding [TACH_CH_TACH0A | TACH_CH_TACH0B] and/or
 * [TACH_CH_TACH1A | TACH_CH_TACH1B]
 */
const struct fan_tach_t fan_tach[] = {
	{TACH_CH_NULL,  -1, -1, -1},
	{TACH_CH_NULL,  -1, -1, -1},
	{TACH_CH_NULL,  -1, -1, -1},
	{TACH_CH_NULL,  -1, -1, -1},
	{TACH_CH_NULL,  -1, -1, -1},
	{TACH_CH_NULL,  -1, -1, -1},
	{TACH_CH_NULL,  -1, -1, -1},
	{TACH_CH_TACH1A, 2, 50, 30},
};
BUILD_ASSERT(ARRAY_SIZE(fan_tach) == PWM_HW_CH_TOTAL);
#endif /* CHIP_FAMILY_IT83XX */

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
	.temp_fan_off = C_TO_K(25),
	.temp_fan_max = C_TO_K(50),
};

struct ec_thermal_config thermal_params[] = {
	[TEMP_SNS_AMBIENT] = thermal_a,
	[TEMP_SNS_BATTERY] = thermal_a,
	[TEMP_SNS_DDR] = thermal_a,
#ifdef CONFIG_PECI
	[TEMP_SNS_PECI] = thermal_a,
#endif
	[TEMP_SNS_SKIN] = thermal_a,
	[TEMP_SNS_VR] = thermal_a,
};
BUILD_ASSERT(ARRAY_SIZE(thermal_params) == TEMP_SENSOR_COUNT);
#endif

static void board_init(void)
{
	/* Enable SOC SPI */
	gpio_set_level(GPIO_EC_SPI_OE_N, 1);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_LAST);

int ioexpander_read_intelrvp_version(int *port0, int *port1)
{
	if (pca9555_read(I2C_PORT_PCA9555_BOARD_ID_GPIO,
		I2C_ADDR_PCA9555_BOARD_ID_GPIO,
		PCA9555_CMD_INPUT_PORT_0, port0))
		return -1;

	return pca9555_read(I2C_PORT_PCA9555_BOARD_ID_GPIO,
		I2C_ADDR_PCA9555_BOARD_ID_GPIO,
		PCA9555_CMD_INPUT_PORT_1, port1);
}
