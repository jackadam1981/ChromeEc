/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * VSYNC driver
 */

#include "accelgyro.h"
#include "config.h"
#include "console.h"
#include "driver/vsync.h"
#include "hwtimer.h"
#include "task.h"

#define CPRINTS(format, args...) cprints(CC_MOTION_SENSE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_MOTION_SENSE, format, ## args)

#ifndef CONFIG_ACCEL_FIFO
#error This driver needs CONFIG_ACCEL_FIFO
#endif

#ifndef CONFIG_ACCEL_INTERRUPTS
#error This driver needs CONFIG_ACCEL_INTERRUPTS
#endif

static uint32_t last_interrupt_timestamp;

static int vsync_read(const struct motion_sensor_t *s, vector_3_t v)
{

	CPRINTF("vsync read\n");

	v[0] = 1;
	v[1] = 0;
	v[2] = 0;

	/*
	 * Return an error when nothing change to prevent filling the
	 * fifo with useless data.
	 */
// 	if (v[0] == drv_data->last_value)
// 		return EC_ERROR_UNCHANGED;
// 	else
		return EC_SUCCESS;
}

static int vsync_set_range(const struct motion_sensor_t *s, int range,
			     int rnd)
{
	CPRINTF("vsync set_data_rate\n");
	return EC_SUCCESS;
}

static int vsync_get_range(const struct motion_sensor_t *s)
{
	CPRINTF("vsync get_range\n");
	return 1;
}

static int vsync_set_data_rate(const struct motion_sensor_t *s,
				int rate, int roundup)
{
	CPRINTF("vsync set_data_rate\n");

	return EC_SUCCESS;
}

static int vsync_get_data_rate(const struct motion_sensor_t *s)
{
	CPRINTF("vsync get_data_rate\n");

	return 1;
}/*

static int vsync_set_offset(const struct motion_sensor_t *s,
			const int16_t *offset,
			int16_t    temp)
{
	return EC_SUCCESS;
}

static int vsync_get_offset(const struct motion_sensor_t *s,
			int16_t   *offset,
			int16_t    *temp)
{
	*offset = 0;

	return EC_SUCCESS;
}*/

/* Bottom half of the irq handler */
void vsync_interrupt(enum gpio_signal signal)
{
	last_interrupt_timestamp = __hw_clock_source_read();
	CPRINTS("vsync!");
	task_set_event(TASK_ID_MOTIONSENSE,
		       CONFIG_ACCELGYRO_BMI160_INT_EVENT, 0);
}

/* Top half of the irq handler */
static int motion_irq_handler(struct motion_sensor_t *s, uint32_t *event)
{
	/*
	 * No need to read the FIFO here, motion sense task is
	 * doing it on every interrupt.
	 */
	return EC_SUCCESS;
}

static int load_fifo(struct motion_sensor_t *s)
{
	struct ec_response_motion_sensor_data vector;

	if (s->type != MOTIONSENSE_TYPE_SYNC)
		return EC_SUCCESS;

	CPRINTF("vsync load_fifo\n");
	vector.flags = 0;
	vector.data[X] = 0;
	vector.data[Y] = 0;
	vector.data[Z] = 0;
	vector.sensor_num = 0;
	motion_sense_fifo_add_data(&vector, s, 3, last_interrupt_timestamp);
	return EC_SUCCESS;
}

/**
 * Initialise BH1730 Ambient light sensor.
 */
static int vsync_init(const struct motion_sensor_t *s)
{
// 	int ret;

	CPRINTF("vsync init\n");

	return 0;
}

#ifdef CONFIG_VSYNC_COMMAND
static int command_vsync(int argc, char **argv)
{
	vsync_interrupt(GPIO_VSYNC_INT);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(vsync, command_vsync,
	NULL,
	"Simulates a vsync event");
#endif

const struct accelgyro_drv vsync_drv = {
	.init = vsync_init,
	.read = vsync_read,
	.set_range = vsync_set_range,
	.get_range = vsync_get_range,
	.set_data_rate = vsync_set_data_rate,
	.get_data_rate = vsync_get_data_rate,
	.irq_handler = motion_irq_handler,
	.load_fifo = load_fifo,
};

