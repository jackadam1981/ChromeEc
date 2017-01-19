/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* STM32L-discovery board configuration */

#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "queue_policies.h"
#include "registers.h"
#include "task.h"
#include "usart-stm32f0.h"
#include "usart_rx_dma.h"
#include "usart_tx_dma.h"
#include "util.h"
#include "i2c.h"
#include "driver/accel_lis2dh.h"
#include "math_util.h"
#include "motion_sense.h"

void sensors_interrupt(enum gpio_signal signal);
void lis2dh_interrupt(enum gpio_signal signal);

#include "gpio_list.h"

/* Mutexes */
static struct mutex g_base_mutex;

/******************************************************************************
 * Setup I2C port where sensors are attached
 */
const struct i2c_port_t i2c_ports[]  = {
	{"sensor", I2C_PORT_MASTER, 100, GPIO_I2C_SCL, GPIO_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/******************************************************************************
 * Motion Sense
 */
/* Matrix to rotate accelrator into standard reference frame */
const matrix_3x3_t base_standard_ref = {
	{ 0, FLOAT_TO_FP(-1), 0 },
	{ FLOAT_TO_FP(1), 0,  0 },
	{ 0, 0,  FLOAT_TO_FP(1) }
};

#ifdef CONFIG_ACCEL_LIS2DH
extern struct stprivate_data lis2dh_a_data;
#endif /* CONFIG_ACCEL_LIS2DH */

struct motion_sensor_t motion_sensors[] = {

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
		.default_range = 2,
		.config = {
			/* AP: by default use EC settings */
			[SENSOR_CONFIG_AP] = {
				.odr = 0,
				.ec_rate = 0,
			},
			/* EC use accel for angle detection */
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 13000 | ROUND_UP_FLAG,
				.ec_rate = 100 * MSEC,
			},
			/* Sensor off in S3/S5 */
			[SENSOR_CONFIG_EC_S3] = {
				.odr = 0,
				.ec_rate = 0
			},
			/* Sensor off in S3/S5 */
			[SENSOR_CONFIG_EC_S5] = {
				.odr = 0,
				.ec_rate = 0
			}
		}
	}

#endif /* CONFIG_ACCEL_LIS2DH */

};

const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

void sensors_interrupt(enum gpio_signal signal)
{
	static int count;

#ifdef CONFIG_ACCEL_LIS2DH
	lis2dh_interrupt(signal);
#endif /* CONFIG_ACCEL_LIS2DH */

	gpio_set_level(GPIO_LED_GREEN, ++count & 0x01);
}

/******************************************************************************
 * Timer Tick Event
 */
void gpio_tick(void)
{
	static int count;

	gpio_set_level(GPIO_LED_BLUE, ++count & 0x01);
}
DECLARE_HOOK(HOOK_TICK, gpio_tick, HOOK_PRIO_DEFAULT);

/******************************************************************************
 * Initialize board.
 */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_USER_BUTTON);
	gpio_set_level(GPIO_LED_BLUE, 0);
	gpio_set_level(GPIO_LED_GREEN, 0);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
