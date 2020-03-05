/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Trembyle board configuration */

#include "button.h"
#include "driver/accel_lis2dw12.h"
#include "driver/accelgyro_lsm6dsm.h"
#include "extpower.h"
#include "fan.h"
#include "fan_chip.h"
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

#include "gpio_list.h"

/*****************************************************************************
 * MSI EC FW Configuration
 */
int ec_config_has_keyboard_backlight(void)
{
	return get_cbi_ec_config_kbblight();
}
int ec_config_has_tablet_mode(void)
{
	return get_cbi_ec_config_tablet();
}

/**
 * CBI_EC_CONFIG_USB_MB_
 *
 * OPT0 USB-A0  Speed: 5 Gbps
 *		Retimer: none
 *	USB-C0  Speed: 5 Gbps
 *		Retimer: none
 *		TCPC: NCT3807
 *		PPC: AOZ1380
 *		IOEX: TCPC
 */
static const uint32_t has_usba0				= BIT(0);
static const uint32_t has_usbc0				= BIT(0);
static const uint32_t has_usbc0_tcpc_nct3807		= BIT(0);
static const uint32_t has_usbc0_ppc_aoz1380		= BIT(0);

int ec_config_has_usba0(void)
{
	return !!(has_usba0 &
		  BIT(get_cbi_ec_config_usb0()));
}
int ec_config_has_usba0_retimer(void)
{
	return 0;
}
int ec_config_has_usbc0(void)
{
	return !!(has_usbc0 &
		  BIT(get_cbi_ec_config_usb0()));
}
int ec_config_has_usbc0_retimer(void)
{
	return 0;
}
int ec_config_has_usbc0_tcpc(void)
{
	return !!(has_usbc0_tcpc_nct3807 &
		  BIT(get_cbi_ec_config_usb0()));
}
int ec_config_has_usbc0_tcpc_nct3807(void)
{
	return !!(has_usbc0_tcpc_nct3807 &
		  BIT(get_cbi_ec_config_usb0()));
}
int ec_config_has_usbc0_ppc(void)
{
	return !!(has_usbc0_ppc_aoz1380 &
		  BIT(get_cbi_ec_config_usb0()));
}
int ec_config_has_usbc0_ppc_aoz1380(void)
{
	return !!(has_usbc0_ppc_aoz1380 &
		  BIT(get_cbi_ec_config_usb0()));
}

/**
 * CBI_EC_CONFIG_USB_DB_
 *
 * OPT0 USB-A1  none
 *	USB-C1  Speed: 5 Gbps
 *		Retimer: TUSB544
 *		TCPC: NCT3807
 *		PPC: NX20P3483
 *		IOEX: TCPC
 *	HDMI    Exists: yes
 *		Retimer: PI3HDX1204
 *		MST Hub: none
 *
 * OPT1 USB-A1  Speed: 5 Gbps
 *		Retimer: TUSB522
 *	USB-C1  Speed: 5 Gbps
 *		Retimer: PS8743
 *		TCPC: NCT3807
 *		PPC: NX20P3483
 *		IOEX: TCPC
 *	HDMI    Exists: no
 *		Retimer: none
 *		MST Hub: none
 */
static const uint32_t has_usba1				=          BIT(1);
static const uint32_t has_usba1_retimer_tusb522		=          BIT(1);
static const uint32_t has_usbc1				= BIT(0) + BIT(1);
static const uint32_t has_usbc1_retimer_ps8743		=          BIT(1);
static const uint32_t has_usbc1_retimer_tusb544		= BIT(0);
static const uint32_t has_usbc1_tcpc_nct3807		= BIT(0) + BIT(1);
static const uint32_t has_usbc1_ppc_nx20p3483		= BIT(0) + BIT(1);
static const uint32_t has_hdmi				= BIT(0);
static const uint32_t has_hdmi_retimer_pi3hdx1204	= BIT(0);

int ec_config_has_usba1(void)
{
	return !!(has_usba1 &
		  BIT(get_cbi_ec_config_usb1()));
}
int ec_config_has_usba1_retimer(void)
{
	return !!(has_usba1_retimer_tusb522 &
		  BIT(get_cbi_ec_config_usb1()));
}
int ec_config_has_usba1_retimer_tusb522(void)
{
	return !!(has_usba1_retimer_tusb522 &
		  BIT(get_cbi_ec_config_usb1()));
}
int ec_config_has_usbc1(void)
{
	return !!(has_usbc1 &
		  BIT(get_cbi_ec_config_usb1()));
}
int ec_config_has_usbc1_retimer(void)
{
	return !!((has_usbc1_retimer_ps8743 | has_usbc1_retimer_tusb544) &
		  BIT(get_cbi_ec_config_usb1()));
}
int ec_config_has_usbc1_retimer_ps8743(void)
{
	return !!(has_usbc1_retimer_ps8743 &
		  BIT(get_cbi_ec_config_usb1()));
}
int ec_config_has_usbc1_retimer_tusb544(void)
{
	return !!(has_usbc1_retimer_tusb544 &
		  BIT(get_cbi_ec_config_usb1()));
}
int ec_config_has_usbc1_tcpc(void)
{
	return !!(has_usbc1_tcpc_nct3807 &
		  BIT(get_cbi_ec_config_usb1()));
}
int ec_config_has_usbc1_tcpc_nct3807(void)
{
	return !!(has_usbc1_tcpc_nct3807 &
		  BIT(get_cbi_ec_config_usb1()));
}
int ec_config_has_usbc1_ppc(void)
{
	return !!(has_usbc1_ppc_nx20p3483 &
		  BIT(get_cbi_ec_config_usb1()));
}
int ec_config_has_usbc1_ppc_nx20p3483(void)
{
	return !!(has_usbc1_ppc_nx20p3483 &
		  BIT(get_cbi_ec_config_usb1()));
}
int ec_config_has_hdmi(void)
{
	return !!(has_hdmi &
		  BIT(get_cbi_ec_config_usb1()));
}
int ec_config_has_hdmi_retimer_pi3hdx1204(void)
{
	return !!(has_hdmi_retimer_pi3hdx1204 &
		  BIT(get_cbi_ec_config_usb1()));
}
int ec_config_has_mst_hub(void)
{
	return 0;
}

/**
 * CBI_EC_CONFIG_LID_ACCEL_
 *
 * OPT0: none
 */
int ec_config_has_lid_accel(void)
{
	return 0;
}

/**
 * CBI_EC_CONFIG_BASE_GYRO_
 *
 * OPT0: none
 */
int ec_config_has_base_gyro(void)
{
	return 0;
}


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
