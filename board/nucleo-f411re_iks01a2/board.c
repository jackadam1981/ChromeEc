/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* nucleo-f411re development board configuration */

#include "common.h"
#include "console.h"
#include "driver/accelgyro_lsm6dsm.h"
#include "driver/accel_lis2de.h"
#include "driver/accel_lis2dh.h"
#include "driver/accel_lis2ds.h"
#include "driver/accel_lis2dw12.h"
#include "driver/baro_lps22hb.h"
#include "driver/mag_lis2mdl.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "motion_sense.h"

#include "gpio.h"
#include "registers.h"
#include "task.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_EVENTS, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_EVENTS, format, ## args)

void user_button_evt(enum gpio_signal signal)
{
	CPRINTF("Button %d, %d!\n", signal, gpio_get_level(signal));
}

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
#ifdef CONFIG_ACCEL_LIS2DH
	lis2dh_interrupt(signal);
#endif /* CONFIG_ACCEL_LIS2DH */

#ifdef CONFIG_ACCELGYRO_LSM6DSM
	lsm6dsm_interrupt(signal);
#endif /* CONFIG_ACCEL_LIS2DH */

#ifdef CONFIG_ACCEL_LIS2DS
	lis2ds_interrupt(signal);
#endif /* CONFIG_ACCEL_LIS2DS */

#ifdef CONFIG_ACCEL_LIS2DE
	lis2de_interrupt(signal);
#endif /* CONFIG_ACCEL_LIS2DE */

#ifdef CONFIG_ACCEL_LIS2DW12
	lis2dw12_interrupt(signal);
#endif /* CONFIG_ACCEL_LIS2DE */
#endif /* CONFIG_ACCEL_INTERRUPTS */
	gpio_set_level(GPIO_USER_LED, 0);
}

#include "gpio_list.h"

/* Initialize board. */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_USER_BUTTON_L);

#ifdef CONFIG_ACCELGYRO_LSM6DSM
	/* Enable interrupts from ST sensors. */
	gpio_enable_interrupt(GPIO_LSM6DSL_INT1);
	gpio_enable_interrupt(GPIO_LSM6DSL_INT2);
#endif
	/* No power control yet */
	/* Go to S3 state */
	hook_notify(HOOK_CHIPSET_STARTUP);

	/* Go to S0 state */
	hook_notify(HOOK_CHIPSET_RESUME);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"master", I2C_PORT_MASTER, 100,
	 GPIO_MASTER_I2C_SCL, GPIO_MASTER_I2C_SDA},
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

#ifdef CONFIG_BARO_LPS22HB
struct stprivate_data lps22hb_p_data;
#endif /* CONFIG_BARO_LPS22HB */

#ifdef CONFIG_ACCEL_LIS2DS
struct stprivate_data lis2ds_a_data;
#endif /* CONFIG_ACCEL_LIS2DS */

#ifdef CONFIG_ACCEL_LIS2DE
struct stprivate_data lis2de_a_data;
#endif /* CONFIG_ACCEL_LIS2DE */

#ifdef CONFIG_ACCEL_LIS2DW12
struct stprivate_data lis2dw12_a_data;
#endif /* CONFIG_ACCEL_LIS2DW12 */

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
		.addr = LSM6DSM_ADDR1,
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
		.addr = LSM6DSM_ADDR1,
		.default_range = 245, /* dps */
		.rot_standard_ref = &base_standard_ref,
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
	},
#endif /* CONFIG_MAG_LIS2MDL */

#ifdef CONFIG_BARO_LPS22HB
	[BASE_BARO] = {
		.name = "Base Baro",
		.active_mask = SENSOR_ACTIVE_S0,
		.chip = MOTIONSENSE_CHIP_LPS22HB,
		.type = MOTIONSENSE_TYPE_BARO,
		.location = MOTIONSENSE_LOC_BASE,
		.drv = &lps22hb_drv,
		.drv_data = &lps22hb_p_data,
		.mutex = &g_base_mutex,
		.port = I2C_PORT_BARO,
		.addr = LPS22HB_ADDR1,
		.default_range = 1 << 18, /*  1bit = 4 Pa, 16bit ~= 2600 hPa */
		.min_frequency = LPS22HB_MIN_ODR,
		.max_frequency = LPS22HB_MAX_ODR,
		.config = {
			/* AP: by default shutdown all sensors */
			[SENSOR_CONFIG_AP] = {
				.odr = 0,
				.ec_rate = 0,
			},
			/* EC does not need in S0 */
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 0,
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
#endif /* CONFIG_BARO_LPS22HB */

#ifdef CONFIG_ACCEL_LIS2DS
	[LID_ACCEL] = {
		.name = "LIS2DS LID",
		.active_mask = SENSOR_ACTIVE_S0,
		.chip = MOTIONSENSE_CHIP_LIS2DS,
		.type = MOTIONSENSE_TYPE_ACCEL,
		.location = MOTIONSENSE_LOC_BASE,
		.drv = &lis2ds_drv,
		.mutex = &g_base_mutex,
		.drv_data = &lis2ds_a_data,
		.port = I2C_PORT_ACCEL,
		.addr = LIS2DS_ADDR0,
		.rot_standard_ref = &base_standard_ref,
		.default_range = 2, /* g, enough for laptop. */
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
#endif /* CONFIG_ACCEL_LIS2DS */

#ifdef CONFIG_ACCEL_LIS2DE
	[LID_ACCEL] = {
		.name = "LIS2DE LID",
		.active_mask = SENSOR_ACTIVE_S0,
		.chip = MOTIONSENSE_CHIP_LIS2DE,
		.type = MOTIONSENSE_TYPE_ACCEL,
		.location = MOTIONSENSE_LOC_BASE,
		.drv = &lis2de_drv,
		.mutex = &g_base_mutex,
		.drv_data = &lis2de_a_data,
		.port = I2C_PORT_ACCEL,
		.addr = LIS2DE_ADDR0,
		.rot_standard_ref = &base_standard_ref,
		.default_range = 2, /* g, enough for laptop. */
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
#endif /* CONFIG_ACCEL_LIS2DE */

#ifdef CONFIG_ACCEL_LIS2DW12
	[LID_ACCEL] = {
		.name = "LIS2DW12 LID",
		.active_mask = SENSOR_ACTIVE_S0,
		.chip = MOTIONSENSE_CHIP_LIS2DW12,
		.type = MOTIONSENSE_TYPE_ACCEL,
		.location = MOTIONSENSE_LOC_BASE,
		.drv = &lis2dw12_drv,
		.mutex = &g_base_mutex,
		.drv_data = &lis2dw12_a_data,
		.port = I2C_PORT_ACCEL,
		.addr = LIS2DW12_ADDR0,
		.rot_standard_ref = &base_standard_ref,
		.default_range = 2,
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
#endif /* CONFIG_ACCEL_LIS2DW12 */
};

const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

void sensor_board_proc_double_tap(void)
{
	/* TODO: Call led update function */
	CPRINTS("Call LED status update");
}
