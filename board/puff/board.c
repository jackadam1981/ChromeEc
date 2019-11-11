/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Puff board-specific configuration */

#include "adc.h"
#include "adc_chip.h"
#include "button.h"
#include "common.h"
#include "cros_board_info.h"
#include "driver/ina3221.h"
#include "driver/ppc/sn5s330.h"
#include "driver/tcpm/anx7447.h"
#include "driver/tcpm/ps8xxx.h"
#include "driver/tcpm/tcpci.h"
#include "ec_commands.h"
#include "extpower.h"
#include "fan.h"
#include "fan_chip.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "spi.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "temp_sensor.h"
#include "thermal.h"
#include "thermistor.h"
#include "uart.h"
#include "usb_charge.h"
#include "usb_pd.h"
#include "usbc_ppc.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static void ppc_interrupt(enum gpio_signal signal)
{
	if (signal == GPIO_USB_C0_TCPPC_INT_ODL)
		sn5s330_interrupt(0);
}

static void tcpc_alert_event(enum gpio_signal signal)
{
	if (signal == GPIO_USB_C0_TCPC_INT_ODL)
		schedule_deferred_pd_interrupt(0);
}

static void port_ocp_interrupt(enum gpio_signal signal)
{
	/* If both HDMI ports are asserting ~FAULT (usually when they're
	 * current-limiting), limit advertised power on the charging ports.
	 */
	int port0_fault = !gpio_get_level(GPIO_HDMI_CONN0_OC_ODL);
	int port1_fault = !gpio_get_level(GPIO_HDMI_CONN1_OC_ODL);

	/* Output high limits power */
	gpio_set_level(GPIO_USB_A_LOW_PWR_OD, port0_fault && port1_fault);
}

static bool usbc_overcurrent = 0;
/* Update USB port power limits based on current state.
 *
 * Port power consumption is assumed to be negligible if not overcurrent,
 * and we have two knobs: the front port power limit, and the USB-C power limit.
 * Either one of the front ports may run at high power (one at a time) or we
 * can limit both to low power. On USB-C we can similarly limit the port power,
 * but there's only one port.
 */
void update_port_limits(void) {
	/* Adjustable current limit settings (high and low), in mA. */
	const int front_hp = 1600;
	const int front_lp = 960;
	const int c_hp = 3740;
	const int c_lp = 2090;

	const enum gpio_signal usb_a_rear_ports[] =
		{GPIO_USB_A2_OC_ODL, GPIO_USB_A3_OC_ODL, GPIO_USB_A4_OC_ODL};
	bool a0_oc, a1_oc, bc_power_limited;
	int rear_oc_count = 0;
	int headroom = 10000;	/* Total 5V rail capacity */
	enum tcpc_rp_value req_c_limit;
	bool req_a_limit;

	/* Base load: things we don't have switches/monitoring for. */
	int pp5000_load = 1335;
	/* Rear USB-A */
	for (size_t i = 0; i < ARRAY_SIZE(usb_a_rear_ports); i++) {
		if (gpio_get_level(usb_a_rear_ports[i]) == 0)
			rear_oc_count += 1;
	}
	pp5000_load += rear_oc_count * 1075;
	/* Front USB-A */
	a0_oc = !gpio_get_level(GPIO_USB_A0_OC_ODL);
	a1_oc = !gpio_get_level(GPIO_USB_A1_OC_ODL);
	bc_power_limited = gpio_get_level(GPIO_USB_A_LOW_PWR_OD);
	if (!bc_power_limited && (a0_oc || a1_oc)) {
		/* At least one charging port may be sinking high current */
		pp5000_load += front_hp;
		/* Only one may run at high power */
		if (a0_oc && a1_oc)
			pp5000_load += front_lp;
	} else if (bc_power_limited) {
		if (a0_oc)
			pp5000_load += front_lp;
		if (a1_oc)
			pp5000_load += front_lp;
	}
	/* Type-C port */
	if (ppc_is_sourcing_vbus(0) && usbc_overcurrent) {
		/* Assume high-power; there's no way to poll this. */
		/* TODO(b/143190102) add a way to poll port power limit so we
		 * can detect when it can be increased again if the port is
		 * already active.
		 */
		pp5000_load += c_hp;
	}

	headroom -= pp5000_load;
	/* Assume any one of the ports that aren't currently in use might become
	 * in use, and we need to be able to power them.
	 */
	if (!ppc_is_sourcing_vbus(0)) {
		/* USB-C not in use, prefer to adjust it. No single other port
		 * can draw as much even if this port is in low-power mode, so
		 * this ensures we can also power any other port that might
		 * become active.
		 */
		if (!bc_power_limited && !(a0_oc || a1_oc)) {
			/* Can reclaim power from a front port if needed. */
			if (headroom > c_hp) {
				/* Can still go full power */
				req_a_limit = 0;
				req_c_limit = TYPEC_RP_3A0;
			} else if (headroom > c_lp) {
				/* Can go lower-power */
				req_a_limit = 0;
				req_c_limit = TYPEC_RP_1A5;
			} else {
				/* Must reduce power on both. */
				req_a_limit = 1;
				req_c_limit = TYPEC_RP_1A5;
			}
		} else {
			/* Neither charging port is limiting, so we have
			 * sufficient budget to allow full power. */
			req_a_limit = 0;
			req_c_limit = TYPEC_RP_3A0;
		}
	} else {
		/* USB-C is in use, prefer to drop front port limits.
		 * Pessimistically Assume C is currently in low-power mode. */
		if (headroom > (c_hp - c_lp + front_hp)) {
			/* Can still go full power */
			req_a_limit = 0;
			req_c_limit = TYPEC_RP_3A0;
		} else if (headroom > (c_hp - c_lp + front_lp)) {
			/* Reducing front allows C to go to full power */
			req_a_limit = 1;
			req_c_limit = TYPEC_RP_3A0;
		} else {
			/* Must reduce both */
			req_a_limit = 1;
			req_c_limit = TYPEC_RP_1A5;
		}
	}

	ppc_set_vbus_source_current_limit(0, req_c_limit);
	/* Output high limits power */
	gpio_set_level(GPIO_USB_A_LOW_PWR_OD, req_a_limit);
}
DECLARE_HOOK(HOOK_INIT, update_port_limits, HOOK_PRIO_DEFAULT);


#include "gpio_list.h" /* Must come after other header files. */

/******************************************************************************/
/* SPI devices */
const struct spi_device_t spi_devices[] = {
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

/******************************************************************************/
/* PWM channels. Must be in the exactly same order as in enum pwm_channel. */
const struct pwm_t pwm_channels[] = {
	[PWM_CH_FAN]        = { .channel = 5,
				.flags = PWM_CONFIG_OPEN_DRAIN,
				.freq = 25000},
	[PWM_CH_LED_RED]    = { .channel = 0,
				.flags = PWM_CONFIG_ACTIVE_LOW |
					 PWM_CONFIG_DSLEEP,
				.freq = 2000 },
	[PWM_CH_LED_GREEN]  = { .channel = 2,
				.flags = PWM_CONFIG_ACTIVE_LOW |
					 PWM_CONFIG_DSLEEP,
				.freq = 2000 },
};

/******************************************************************************/
/* USB-C TCPC Configuration */
const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
};
struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_MAX_COUNT] = {
};

/******************************************************************************/
/* I2C port map configuration */
const struct i2c_port_t i2c_ports[] = {
	{"ina",     I2C_PORT_INA,     100, GPIO_I2C0_SCL, GPIO_I2C0_SDA},
	{"ppc0",    I2C_PORT_PPC0,    100, GPIO_I2C1_SCL, GPIO_I2C1_SDA},
	{"tcpc0",   I2C_PORT_TCPC0,   100, GPIO_I2C3_SCL, GPIO_I2C3_SDA},
	{"power",   I2C_PORT_POWER,   100, GPIO_I2C5_SCL, GPIO_I2C5_SDA},
	{"eeprom",  I2C_PORT_EEPROM,  100, GPIO_I2C7_SCL, GPIO_I2C7_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

const struct adc_t adc_channels[] = {
	[ADC_SNS_PP3300] = {  /* 9/11 voltage divider */
		.name = "SNS_PP3300",
		.input_ch = NPCX_ADC_CH2,
		.factor_mul = ADC_MAX_VOLT * 11,
		.factor_div = (ADC_READ_MAX + 1) * 9,
	},
	[ADC_SNS_PP1050] = {
		.name = "SNS_PP1050",
		.input_ch = NPCX_ADC_CH7,
		.factor_mul = ADC_MAX_VOLT,
		.factor_div = ADC_READ_MAX + 1,
	},
	[ADC_VBUS] = {  /* 5/39 voltage divider */
		.name = "VBUS",
		.input_ch = NPCX_ADC_CH4,
		.factor_mul = ADC_MAX_VOLT * 39,
		.factor_div = (ADC_READ_MAX + 1) / 5,
	},
	[ADC_PPVAR_IMON] = {  /* 500 mV/A */
		.name = "PPVAR_IMON",
		.input_ch = NPCX_ADC_CH9,
		.factor_mul = ADC_MAX_VOLT,
		.factor_div = ADC_READ_MAX + 1,
	},
	[ADC_TEMP_SENSOR_1] = {
		.name = "TEMP_SENSOR_1",
		.input_ch = NPCX_ADC_CH0,
		.factor_mul = ADC_MAX_VOLT,
		.factor_div = ADC_READ_MAX + 1,
	},
	[ADC_TEMP_SENSOR_2] = {
		.name = "TEMP_SENSOR_2",
		.input_ch = NPCX_ADC_CH1,
		.factor_mul = ADC_MAX_VOLT,
		.factor_div = ADC_READ_MAX + 1,
	},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

const struct temp_sensor_t temp_sensors[] = {
	[TEMP_SENSOR_PP3300] = {
		.name = "PP3300",
		.type = TEMP_SENSOR_TYPE_BOARD,
		.read = get_temp_3v3_30k9_47k_4050b,
		.idx = ADC_TEMP_SENSOR_1,
		.action_delay_sec = 1,
	},
	[TEMP_SENSOR_PP5000] = {
		.name = "PP5000",
		.type = TEMP_SENSOR_TYPE_BOARD,
		.read = get_temp_3v3_30k9_47k_4050b,
		.idx = ADC_TEMP_SENSOR_2,
		.action_delay_sec = 1,
	},
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);

/******************************************************************************/
/* Wake up pins */
const enum gpio_signal hibernate_wake_pins[] = {
};
const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);

/******************************************************************************/
/* Physical fans. These are logically separate from pwm_channels. */
const struct fan_conf fan_conf_0 = {
	.flags = FAN_USE_RPM_MODE,
	.ch = MFT_CH_0,	/* Use MFT id to control fan */
	.pgood_gpio = -1,
	.enable_gpio = -1,
};

const struct fan_rpm fan_rpm_0 = {
	.rpm_min = 3200,
	.rpm_start = 3200,
	.rpm_max = 6500,
};

const struct fan_t fans[] = {
	[FAN_CH_0] = { .conf = &fan_conf_0, .rpm = &fan_rpm_0, },
};
BUILD_ASSERT(ARRAY_SIZE(fans) == FAN_CH_COUNT);

/******************************************************************************/
/* MFT channels. These are logically separate from pwm_channels. */
const struct mft_t mft_channels[] = {
	[MFT_CH_0] = {NPCX_MFT_MODULE_1, TCKC_LFCLK, PWM_CH_FAN},
};
BUILD_ASSERT(ARRAY_SIZE(mft_channels) == MFT_CH_COUNT);

/******************************************************************************/
/* Thermal control; drive fan based on temperature sensors. */
const static struct ec_thermal_config thermal_a = {
	.temp_host = {
		[EC_TEMP_THRESH_WARN] = 0,
		[EC_TEMP_THRESH_HIGH] = C_TO_K(65),
		[EC_TEMP_THRESH_HALT] = C_TO_K(75),
	},
	.temp_host_release = {
		[EC_TEMP_THRESH_WARN] = 0,
		[EC_TEMP_THRESH_HIGH] = C_TO_K(55),
		[EC_TEMP_THRESH_HALT] = 0,
	},
	.temp_fan_off = C_TO_K(25),
	.temp_fan_max = C_TO_K(55),
};

struct ec_thermal_config thermal_params[] = {
	[TEMP_SENSOR_PP3300] = thermal_a,
	[TEMP_SENSOR_PP5000] = thermal_a,
};
BUILD_ASSERT(ARRAY_SIZE(thermal_params) == TEMP_SENSOR_COUNT);

/* Power sensors */
const struct ina3221_t ina3221[] = {
	{ I2C_PORT_INA, 0x40, { "PP3300_G", "PP5000_A", "PP3300_WLAN" } },
	{ I2C_PORT_INA, 0x42, { "PP3300_A", "PP3300_SSD", "PP3300_LAN" } },
	{ I2C_PORT_INA, 0x43, { NULL, "PP1200_U", "PP2500_DRAM" } }
};
const unsigned int ina3221_count = ARRAY_SIZE(ina3221);

static void board_init(void)
{
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/******************************************************************************/
/* USB-C PPC Configuration */
struct ppc_config_t ppc_chips[CONFIG_USB_PD_PORT_MAX_COUNT] = {
};
unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);

/* USB-A port control */
const int usb_port_enable[USB_PORT_COUNT] = {
	GPIO_EN_PP5000_USB_VBUS,
};

/* Power Delivery and charging functions */
void baseboard_tcpc_init(void)
{
	/* Only reset TCPC if not sysjump */
	if (!system_jumped_to_this_image())
		board_reset_pd_mcu();

}
DECLARE_HOOK(HOOK_INIT, baseboard_tcpc_init, HOOK_PRIO_INIT_I2C + 1);

int64_t get_time_dsw_pwrok(void)
{
	/* DSW_PWROK is turned on before EC was powered. */
	return -20 * MSEC;
}

uint16_t tcpc_get_alert_status(void)
{
	uint16_t status = 0;

	return status;
}

void board_reset_pd_mcu(void)
{
}

int board_set_active_charge_port(int port)
{
	return EC_SUCCESS;
}

int ppc_get_alert_status(int port)
{
	return 0;
}

void board_set_charge_limit(int port, int supplier, int charge_ma,
			    int max_ma, int charge_mv)
{
}

void board_overcurrent_event(int port, int is_overcurrented)
{
	/* Sanity check the port. */
	if ((port < 0) || (port >= CONFIG_USB_PD_PORT_MAX_COUNT))
		return;
	usbc_overcurrent = is_overcurrented;
}
