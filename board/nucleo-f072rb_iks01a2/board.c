/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* nucleo-f411re development board configuration */

#include "common.h"
#include "console.h"
#include "driver/accelgyro_lsm6dsm.h"
#include "driver/accel_lis2dh.h"
#include "driver/accel_lis2dw12.h"
#include "driver/mag_lis2mdl.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "motion_sense.h"

#include "gpio.h"
#include "registers.h"
#include "task.h"
#include "i2c.h"
#include "timer.h"

void button_event(enum gpio_signal signal)
{
	gpio_set_level(GPIO_LED_U, 1);
}

/*
 * Interrupt handler for external mems.
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

#ifdef CONFIG_ACCEL_LIS2DW12
	lis2dw12_interrupt(signal);
#endif /* CONFIG_ACCEL_LIS2DW12 */
#endif /* CONFIG_ACCEL_INTERRUPTS */
	gpio_set_level(GPIO_LED_U, 0);
}

#include "gpio_list.h"

#if 0
/* For debugging the LED */
void tick_event(void)
{
	static int count;

	gpio_set_level(GPIO_LED_U, (count & 0x07) == 0);

	count++;
}
DECLARE_HOOK(HOOK_TICK, tick_event, HOOK_PRIO_DEFAULT);
#endif

/* Initialize board. */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_USER_BUTTON_L);

#ifdef CONFIG_ACCELGYRO_LSM6DSM
	/* Enable interrupts from ST sensors. */
	gpio_enable_interrupt(GPIO_LSM6DSL_INT1);
#endif
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
const mat33_fp_t base_standard_ref = {
	{ 0, FLOAT_TO_FP(-1), 0 },
	{ FLOAT_TO_FP(1), 0,  0 },
	{ 0, 0,  FLOAT_TO_FP(1) }
};

#ifdef CONFIG_ACCELGYRO_LSM6DSM
static struct lsm6dsm_data lsm6dsm_a_data = LSM6DSM_DATA;
#endif /* CONFIG_ACCELGYRO_LSM6DSM */

#ifdef CONFIG_ACCEL_LIS2DH
static struct stprivate_data lis2dh_a_data;
#endif /* CONFIG_ACCEL_LIS2DH */

#ifdef CONFIG_MAG_LIS2MDL
#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL
#error "not implemented"
#else /* CONFIG_MAG_LSM6DSM_LIS2MDL */
static struct lis2mdl_private_data lis2mdl_a_data;
#endif /*  CONFIG_MAG_LSM6DSM_LIS2MDL */
#endif /* CONFIG_MAG_LIS2MDL */

#ifdef CONFIG_ACCEL_LIS2DW12
struct stprivate_data lis2dw12_a_data;
#endif /* CONFIG_ACCEL_LIS2DW12 */

struct motion_sensor_t motion_sensors[] = {
#ifdef CONFIG_ACCELGYRO_LSM6DSM
	[BASE_ACCEL] = {
		.name = "LSM6DSM ACC",
		.active_mask = SENSOR_ACTIVE_S0_S3_S5,
		.chip = MOTIONSENSE_CHIP_LSM6DSM,
		.type = MOTIONSENSE_TYPE_ACCEL,
		.location = MOTIONSENSE_LOC_BASE,
		.drv = &lsm6dsm_drv,
		.mutex = &g_base_mutex,
		.drv_data = LSM6DSM_ST_DATA(lsm6dsm_a_data,
				MOTIONSENSE_TYPE_ACCEL),
		.int_signal = GPIO_LSM6DSL_INT1,
		.flags = MOTIONSENSE_FLAG_INT_SIGNAL,
		.i2c_spi_addr_flags = LSM6DSM_ADDR1_FLAGS,
		.port = I2C_PORT_MASTER,
		.rot_standard_ref = &base_standard_ref,
		.default_range = 2, /* g, enough for laptop. */
		.min_frequency = LSM6DSM_ODR_MIN_VAL,
		.max_frequency = LSM6DSM_ODR_MAX_VAL,
		.config = {
			/* EC use accel for angle detection */
			[SENSOR_CONFIG_EC_S5] = {
				.odr = 13000,
			},
		},
	},
	[BASE_GYRO] = {
		.name = "LSM6DSM GYRO",
		.active_mask = SENSOR_ACTIVE_S0_S3_S5,
		.chip = MOTIONSENSE_CHIP_LSM6DSM,
		.type = MOTIONSENSE_TYPE_GYRO,
		.location = MOTIONSENSE_LOC_BASE,
		.drv = &lsm6dsm_drv,
		.mutex = &g_base_mutex,
		.drv_data = LSM6DSM_ST_DATA(lsm6dsm_a_data,
				MOTIONSENSE_TYPE_GYRO),
		.int_signal = GPIO_LSM6DSL_INT1,
		.flags = MOTIONSENSE_FLAG_INT_SIGNAL,
		.i2c_spi_addr_flags = LSM6DSM_ADDR1_FLAGS,
		.port = I2C_PORT_MASTER,
		.default_range = 245, /* dps */
		.rot_standard_ref = &base_standard_ref,
		.min_frequency = LSM6DSM_ODR_MIN_VAL,
		.max_frequency = LSM6DSM_ODR_MAX_VAL,
		.config = {
			/* EC does not need in S0 */
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 13000,
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
		.port = I2C_PORT_MASTER,
		.i2c_spi_addr_flags = LIS2DH_ADDR0_FLAGS,
		.rot_standard_ref = &base_standard_ref,
		.default_range = 2, /* g, enough for laptop. */
		.min_frequency = LIS2DH_ODR_MIN_VAL,
		.max_frequency = LIS2DH_ODR_MAX_VAL,
	},
#endif /* CONFIG_ACCEL_LIS2DH */

#ifdef CONFIG_MAG_LIS2MDL
	[BASE_MAG] = {
		.name = "LIS2MDL MAG",
		.active_mask = SENSOR_ACTIVE_S0_S3_S5,
		.chip = MOTIONSENSE_CHIP_LIS2MDL,
		.type = MOTIONSENSE_TYPE_MAG,
		.location = MOTIONSENSE_LOC_BASE,

#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL
		/* In case MAG is managed by lsm6dsm. */
		.drv = &lsm6dsm_drv,
		.drv_data = &lis2mdl_m_data,
		.i2c_spi_addr_flags = LIS2MDL_ADDR0_FLAGS,
#else /* CONFIG_MAG_LSM6DSM_LIS2MDL */
		/* In case MAG is stand alone device. */
		.drv = &lis2mdl_drv,
		.drv_data = LIS2MDL_ST_DATA(lis2mdl_a_data),
		.i2c_spi_addr_flags = LIS2MDL_ADDR_FLAGS,
#endif /* CONFIG_MAG_LSM6DSM_LIS2MDL */
		.mutex = &g_base_mutex,
		.port = I2C_PORT_MASTER,
		.default_range = 1 << 11,	/* 16LSB / uT, fixed  */
		.rot_standard_ref = NULL,
		.config = {
			/* For testing. */
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 13000,
				.ec_rate = 100,
			},
		},
	},
#endif /* CONFIG_MAG_LIS2MDL */
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
		.port = I2C_PORT_MASTER,
		.i2c_spi_addr_flags = LIS2DW12_ADDR0_FLAGS,
		.rot_standard_ref = &base_standard_ref,
		.default_range = 2,
	},
#endif /* CONFIG_ACCEL_LIS2DW12 */
};

const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

void sensor_board_proc_double_tap(void)
{
	/* TODO: Call led update function */
	ccprintf("Call LED status update");
}
