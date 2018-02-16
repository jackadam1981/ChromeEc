/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* STM32L-discovery board configuration */

#include "common.h"
#include "console.h"
#include "driver/accelgyro_lsm6dsm.h"
#include "driver/accel_lis2dh.h"
#include "driver/mag_lis2mdl.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "math_util.h"
#include "motion_sense.h"
#include "queue_policies.h"
#include "registers.h"
#include "task.h"
#include "usart-stm32f0.h"
#include "usart_rx_dma.h"
#include "usart_tx_dma.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

/*
 * Interrupt handler for external mems int1/2
 *
 * Dispatch interrupt (FIFO, data ready and so on)
 * to service routine and toggle green led on discovery
 * board.
 *
 * NOTE: Interrupt handler is shared by multiple devices so
 * need to be enable once a time (LIS2DH/LSM6DSM etc.)
 * to be sure it works correctly.
 */
void sensors_interrupt(enum gpio_signal signal)
{

#ifdef CONFIG_ACCEL_INTERRUPTS
	static int count;

#ifdef CONFIG_ACCEL_LIS2DH
	lis2dh_interrupt(signal);
#endif /* CONFIG_ACCEL_LIS2DH */

#ifdef CONFIG_ACCELGYRO_LSM6DSM
	lsm6dsm_interrupt(signal);
#endif /* CONFIG_ACCEL_LIS2DH */

	gpio_set_level(GPIO_LED_GREEN, ++count & 0x01);
#endif /* CONFIG_ACCEL_INTERRUPTS */
}

#include "gpio_list.h"


/* Setup I2C port where sensors are attached */
const struct i2c_port_t i2c_ports[]  = {
	{"sensor", I2C_PORT_MASTER, 100, GPIO_I2C_SCL, GPIO_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* Base Sensor mutex */
static struct mutex g_base_mutex;

/*
 * Motion Sense
 * Matrix to rotate accelrator into standard reference frame
 */
const matrix_3x3_t base_standard_ref = {
	{ 0, FLOAT_TO_FP(-1), 0 },
	{ FLOAT_TO_FP(1), 0,  0 },
	{ 0, 0,  FLOAT_TO_FP(1) }
};

#ifdef CONFIG_ACCELGYRO_LSM6DSM
struct stprivate_data lsm6dsm_a_data;
struct stprivate_data lsm6dsm_g_data;
#endif /* CONFIG_ACCELGYRO_LSM6DSM */

#ifdef CONFIG_ACCEL_LIS2DH
struct stprivate_data lis2dh_a_data;
struct stprivate_data lis2dh_l_data;
#endif /* CONFIG_ACCEL_LIS2DH */

#ifdef CONFIG_MAG_LIS2MDL
#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL
struct stprivate_data lsm6dsm_m_data;
#else /* CONFIG_MAG_LSM6DSM_LIS2MDL */
struct stprivate_data lis2mdl_m_data;
#endif /*  CONFIG_MAG_LSM6DSM_LIS2MDL */
#endif /* CONFIG_MAG_LIS2MDL */

struct motion_sensor_t motion_sensors[] = {
#ifdef CONFIG_ACCELGYRO_LSM6DSM

	[BASE_ACCEL] = {
		.name = "LSM6DSM ACC",
		.active_mask = SENSOR_ACTIVE_S0,
		.chip = MOTIONSENSE_CHIP_LSM6DSM,
		.type = MOTIONSENSE_TYPE_ACCEL,
		.location = MOTIONSENSE_LOC_BASE,
		.drv = &lsm6dsm_drv,
		.mutex = &g_base_mutex,
		.drv_data = &lsm6dsm_a_data,
		.port = I2C_PORT_ACCEL,
		.addr = LSM6DSM_ADDR0,
		.rot_standard_ref = &base_standard_ref,
		.default_range = 2, /* g, enough for laptop. */
		.min_frequency = LSM6DSM_ODR_MIN_VAL,
		.max_frequency = LSM6DSM_ODR_MAX_VAL,
		.config = {
			/* AP: by default use EC settings */
			[SENSOR_CONFIG_AP] = {
				.odr = 0,
				.ec_rate = 0,
			},
			/* EC use accel for angle detection */
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 13000,
				.ec_rate = 13 * MSEC,
			},
			/* Sensor off in S5 */
			[SENSOR_CONFIG_EC_S3] = {
				.odr = 0,
				.ec_rate = 0
			},
			/* Sensor off in S5 */
			[SENSOR_CONFIG_EC_S5] = {
				.odr = 0,
				.ec_rate = 0
			},
		},
	},

	[BASE_GYRO] = {
		.name = "LSM6DSM GYRO",
		.active_mask = SENSOR_ACTIVE_S0,
		.chip = MOTIONSENSE_CHIP_LSM6DSM,
		.type = MOTIONSENSE_TYPE_GYRO,
		.location = MOTIONSENSE_LOC_BASE,
		.drv = &lsm6dsm_drv,
		.mutex = &g_base_mutex,
		.drv_data = &lsm6dsm_g_data,
		.port = I2C_PORT_GYRO,
		.addr = LSM6DSM_ADDR0,
		.rot_standard_ref = &base_standard_ref,
		.default_range = 245, /* dps */
		.min_frequency = LSM6DSM_ODR_MIN_VAL,
		.max_frequency = LSM6DSM_ODR_MAX_VAL,
		.config = {
			/* AP: by default shutdown all sensors */
			[SENSOR_CONFIG_AP] = {
				.odr = 0,
				.ec_rate = 0,
			},
			/* EC does not need in S0 */
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 13000,
				.ec_rate = 0,
			},
			/* Sensor off in S3/S5 */
			[SENSOR_CONFIG_EC_S3] = {
				.odr = 0,
				.ec_rate = 0,
			},
			/* Sensor off in S3/S5 */
			[SENSOR_CONFIG_EC_S5] = {
				.odr = 0,
				.ec_rate = 0,
			},
		},
	},

#endif /* CONFIG_ACCELGYRO_LSM6DSM */

#ifdef CONFIG_ACCEL_LIS2DH
	[LID_ACCEL] = {
		.name = "LIS2DH BASE",
		.active_mask = SENSOR_ACTIVE_S0,
		.chip = MOTIONSENSE_CHIP_LIS2DH,
		.type = MOTIONSENSE_TYPE_ACCEL,
		.location = MOTIONSENSE_LOC_BASE,
		.drv = &lis2dh_drv,
		.mutex = &g_base_mutex,
		.drv_data = &lis2dh_a_data,
		.port = I2C_PORT_ACCEL,
		.addr = LIS2DH_ADDR0,
		.rot_standard_ref = &base_standard_ref,
		.default_range = 2, /* g, enough for laptop. */
		.min_frequency = LIS2DH_ODR_MIN_VAL,
		.max_frequency = LIS2DH_ODR_MAX_VAL,
		.config = {
			/* AP: by default use EC settings */
			[SENSOR_CONFIG_AP] = {
				.odr = 0,
				.ec_rate = 0,
			},
			/* EC use accel for angle detection */
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 0,
				.ec_rate = 0,
			},
			/* Sensor off in S3 */
			[SENSOR_CONFIG_EC_S3] = {
				.odr = 0,
				.ec_rate = 0
			},
			/* Sensor off in S5 */
			[SENSOR_CONFIG_EC_S5] = {
				.odr = 0,
				.ec_rate = 0
			},
		},
	},
#endif /* CONFIG_ACCEL_LIS2DH */

#ifdef CONFIG_MAG_LIS2MDL
	[BASE_MAG] = {
		.name = "LIS2MDL MAG",
		.active_mask = SENSOR_ACTIVE_S0,
		.chip = MOTIONSENSE_CHIP_LIS2MDL,
		.type = MOTIONSENSE_TYPE_MAG,
		.location = MOTIONSENSE_LOC_BASE,

#ifndef CONFIG_MAG_LSM6DSM_LIS2MDL
		/* In case MAG is stand alone device. */
		.drv = &lis2mdl_drv,
		.drv_data = &lis2mdl_m_data,
#else /* CONFIG_MAG_LSM6DSM_LIS2MDL */
		/* In case MAG is managed by lsm6dsm. */
		.drv = &lsm6dsm_drv,
		.drv_data = &lsm6dsm_m_data,
#endif /* CONFIG_MAG_LSM6DSM_LIS2MDL */

		.mutex = &g_base_mutex,
		.port = I2C_PORT_ACCEL,
		.addr = LIS2MDL_ADDR0,
		.default_range = LIS2MDL_RANGE,
		.rot_standard_ref = NULL,
		.config = {
			/* AP: by default shutdown all sensors */
			[SENSOR_CONFIG_AP] = {
				.odr = 0,
				.ec_rate = 0,
			},
			/* EC does not need in S0 */
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 13000,
				.ec_rate = 13 * MSEC,
			},
			/* Sensor off in S3/S5 */
			[SENSOR_CONFIG_EC_S3] = {
				.odr = 0,
				.ec_rate = 0,
			},
			/* Sensor off in S3/S5 */
			[SENSOR_CONFIG_EC_S5] = {
				.odr = 0,
				.ec_rate = 0,
			},
		},
	}
#endif /* CONFIG_MAG_LIS2MDL */

};

const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

void sensor_board_proc_double_tap(void)
{
	CPRINTS("Call LED status update");
}

/* Blue Led Blink on Timer Tick Event. */
void gpio_tick(void)
{
	static int count;

	gpio_set_level(GPIO_LED_BLUE, ++count & 0x01);
}
DECLARE_HOOK(HOOK_TICK, gpio_tick, HOOK_PRIO_DEFAULT);

/* Initialize board */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_USER_BUTTON);
	gpio_set_level(GPIO_LED_BLUE, 0);
	gpio_set_level(GPIO_LED_GREEN, 0);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
