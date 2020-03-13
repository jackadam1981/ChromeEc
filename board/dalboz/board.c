/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Trembyle board configuration */

#include "button.h"
#include "driver/accel_lis2dw12.h"
#include "driver/accelgyro_lsm6dsm.h"
#include "driver/ioexpander/pcal6408.h"
#include "extpower.h"
#include "fan.h"
#include "fan_chip.h"
#include "hooks.h"
#include "ioexpander.h"
#include "gpio.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "usb_charge.h"
#include "usb_pd.h"

#include "gpio_list.h"

#ifdef HAS_TASK_MOTIONSENSE

/* Motion sensors */
static struct mutex g_lid_mutex;
static struct mutex g_base_mutex;

/* sensor private data */
static struct stprivate_data g_lis2dwl_data;
static struct lsm6dsm_data g_lsm6dsm_data = LSM6DSM_DATA;

/* Matrix to rotate accelrator into standard reference frame */
static const mat33_fp_t base_standard_ref = {
	{ FLOAT_TO_FP(-1), 0, 0},
	{ 0, FLOAT_TO_FP(-1), 0},
	{ 0, 0, FLOAT_TO_FP(1)}
};

/* TODO(gcc >= 5.0) Remove the casts to const pointer at rot_standard_ref */
struct motion_sensor_t motion_sensors[] = {
	[LID_ACCEL] = {
	 .name = "Lid Accel",
	 .active_mask = SENSOR_ACTIVE_S0_S3,
	 .chip = MOTIONSENSE_CHIP_LIS2DWL,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_LID,
	 .drv = &lis2dw12_drv,
	 .mutex = &g_lid_mutex,
	 .drv_data = &g_lis2dwl_data,
	 .port = I2C_PORT_SENSOR,
	 .i2c_spi_addr_flags = LIS2DWL_ADDR1_FLAGS,
	 .rot_standard_ref = NULL,
	 .default_range = 2, /* g, enough for laptop. */
	 .min_frequency = LIS2DW12_ODR_MIN_VAL,
	 .max_frequency = LIS2DW12_ODR_MAX_VAL,
	 .config = {
		 /* EC use accel for angle detection */
		[SENSOR_CONFIG_EC_S0] = {
			.odr = 12500 | ROUND_UP_FLAG,
		},
		 /* Sensor on for lid angle detection */
		[SENSOR_CONFIG_EC_S3] = {
			.odr = 10000 | ROUND_UP_FLAG,
		},
	},
	},

	[BASE_ACCEL] = {
	 .name = "Base Accel",
	 .active_mask = SENSOR_ACTIVE_S0_S3,
	 .chip = MOTIONSENSE_CHIP_LSM6DSM,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_BASE,
	 .drv = &lsm6dsm_drv,
	 .mutex = &g_base_mutex,
	 .drv_data = LSM6DSM_ST_DATA(g_lsm6dsm_data,
			MOTIONSENSE_TYPE_ACCEL),
	 .int_signal = GPIO_6AXIS_INT_L,
	 .flags = MOTIONSENSE_FLAG_INT_SIGNAL,
	 .port = I2C_PORT_SENSOR,
	 .i2c_spi_addr_flags = LSM6DSM_ADDR0_FLAGS,
	 .default_range = 4, /* g, enough for laptop */
	 .rot_standard_ref = &base_standard_ref,
	 .min_frequency = LSM6DSM_ODR_MIN_VAL,
	 .max_frequency = LSM6DSM_ODR_MAX_VAL,
	 .config = {
		 /* EC use accel for angle detection */
		[SENSOR_CONFIG_EC_S0] = {
			.odr = 13000 | ROUND_UP_FLAG,
			.ec_rate = 100 * MSEC,
		},
		/* Sensor on for angle detection */
		[SENSOR_CONFIG_EC_S3] = {
			.odr = 10000 | ROUND_UP_FLAG,
			.ec_rate = 100 * MSEC,
		},
	 },
	},

	[BASE_GYRO] = {
	 .name = "Base Gyro",
	 .active_mask = SENSOR_ACTIVE_S0_S3,
	 .chip = MOTIONSENSE_CHIP_LSM6DSM,
	 .type = MOTIONSENSE_TYPE_GYRO,
	 .location = MOTIONSENSE_LOC_BASE,
	 .drv = &lsm6dsm_drv,
	 .mutex = &g_base_mutex,
	 .drv_data = LSM6DSM_ST_DATA(g_lsm6dsm_data,
			MOTIONSENSE_TYPE_GYRO),
	.int_signal = GPIO_6AXIS_INT_L,
	.flags = MOTIONSENSE_FLAG_INT_SIGNAL,
	 .port = I2C_PORT_SENSOR,
	 .i2c_spi_addr_flags = LSM6DSM_ADDR0_FLAGS,
	 .default_range = 1000 | ROUND_UP_FLAG, /* dps */
	 .rot_standard_ref = &base_standard_ref,
	 .min_frequency = LSM6DSM_ODR_MIN_VAL,
	 .max_frequency = LSM6DSM_ODR_MAX_VAL,
	},
};

unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

#endif /* HAS_TASK_MOTIONSENSE */

void board_update_sensor_config_from_sku(void)
{
	/* Enable Gyro interrupts */
	gpio_enable_interrupt(GPIO_6AXIS_INT_L);
}

const struct pwm_t pwm_channels[] = {
	[PWM_CH_KBLIGHT] = {
		.channel = 3,
		.flags = PWM_CONFIG_DSLEEP,
		.freq = 100,
	},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

#define ALTERNATE_IOEX_USBC 0
#define ALTERNATE_IOEX_HDMI 1

__override int board_get_ioex_altgrp(int port, uint8_t *alt_grp)
{
	if (port == USBC_PORT_C0) {
		*alt_grp = ALTERNATE_IOEX_USBC;
		return EC_SUCCESS;
	} else {
		if (ec_config_get_usb_db() == DALBOZ_DB_D_OPT2_USBA_HDMI)
			*alt_grp =  ALTERNATE_IOEX_HDMI;
		else
			*alt_grp = ALTERNATE_IOEX_USBC;
	}

	return EC_SUCCESS;
}

/* These IO expander GPIOs vary with DB option. */
enum gpio_signal IOEX_USB_A1_RETIMER_EN = IOEX_USB_A1_RETIMER_EN_OPT1;
enum gpio_signal IOEX_EN_USB_A1_5V_DB = IOEX_EN_USB_A1_5V_DB_OPT1;
enum gpio_signal IOEX_USB_A1_CHARGE_EN_DB_L = IOEX_USB_A1_CHARGE_EN_DB_L_OPT1;
enum gpio_signal GPIO_USB2_ILIM_SEL = IOEX_USB_A1_CHARGE_EN_DB_L_OPT1;

extern int usb_port_enable[USB_PORT_COUNT];

void board_update_ioex_config(void)
{
	ccprints("ec_config_get_usb_db is %d", ec_config_get_usb_db());
	if (ec_config_get_usb_db() == DALBOZ_DB_D_OPT2_USBA_HDMI) {
		ioex_config[USBC_PORT_C1].i2c_slave_addr = PCAL6408_I2C_ADDR0;
		ioex_config[USBC_PORT_C1].drv = &pcal6408_ioexpander_drv;

		IOEX_USB_A1_RETIMER_EN = IOEX_USB_A1_RETIMER_EN_OPT2;
		IOEX_EN_USB_A1_5V_DB = IOEX_EN_USB_A1_5V_DB_OPT2;
		IOEX_USB_A1_CHARGE_EN_DB_L = IOEX_USB_A1_CHARGE_EN_DB_L_OPT2;

		usb_port_enable[1] = IOEX_EN_USB_A1_5V_DB_OPT2;
		GPIO_USB2_ILIM_SEL = IOEX_USB_A1_CHARGE_EN_DB_L_OPT2;
	}

	return;
}

static void pcal6408_ioex_int_handler(void)
{
	pcal6408_ioex_event_handler(1);
}
DECLARE_DEFERRED(pcal6408_ioex_int_handler);

void tcpc_alert_event(enum gpio_signal signal)
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

	if (ec_config_get_usb_db() == DALBOZ_DB_D_OPT2_USBA_HDMI) {
		if (port == 0)
			schedule_deferred_pd_interrupt(port);
		else if (port == 1)
			hook_call_deferred(&pcal6408_ioex_int_handler_data, 0);
	} else
		schedule_deferred_pd_interrupt(port);
}

static void board_hdmi_enable_interrupt(void)
{
	/* Enable HPD interrupts */
	if (ec_config_get_usb_db() == DALBOZ_DB_D_OPT2_USBA_HDMI)
		ioex_enable_interrupt(IOEX_HDMI_CONN_HPD_3V3_DB_OPT2);
}
DECLARE_HOOK(HOOK_INIT, board_hdmi_enable_interrupt, HOOK_PRIO_INIT_I2C + 2);
