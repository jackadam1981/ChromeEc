/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * VSYNC driver
 */

#include "accelgyro.h"
#include "config.h"
#include "console.h"
#include "driver/sync.h"
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
static int counter;

static int sync_read(const struct motion_sensor_t *s, vector_3_t v)
{

	CPRINTF("sync read\n");

	v[0] = counter;
	v[1] = 0;
	v[2] = 0;

	return EC_SUCCESS;
}

static int sync_set_data_rate(const struct motion_sensor_t *s,
				int rate, int roundup)
{
	CPRINTF("sync set_data_rate\n");

	return EC_SUCCESS;
}

static int sync_get_data_rate(const struct motion_sensor_t *s)
{
	CPRINTF("sync get_data_rate\n");

	return 1;
}/*

static int sync_set_offset(const struct motion_sensor_t *s,
			const int16_t *offset,
			int16_t    temp)
{
	return EC_SUCCESS;
}

static int sync_get_offset(const struct motion_sensor_t *s,
			int16_t   *offset,
			int16_t    *temp)
{
	*offset = 0;

	return EC_SUCCESS;
}*/

/* Bottom half of the irq handler */
void sync_interrupt(enum gpio_signal signal)
{
	last_interrupt_timestamp = __hw_clock_source_read();
	counter++;
	CPRINTS("sync!");
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
	static uint32_t previous_interrupt_timestamp;
	uint32_t timestamp;

	/* this should be the atomic read */
	timestamp = last_interrupt_timestamp;

	if (previous_interrupt_timestamp == timestamp)
		return EC_SUCCESS; /* nothing new yet */

	previous_interrupt_timestamp = timestamp;

	CPRINTF("sync load_fifo\n");
	vector.flags = 0;
	vector.data[X] = counter;
	vector.data[Y] = 0;
	vector.data[Z] = 0;
	vector.sensor_num = 0;
	motion_sense_fifo_add_data(&vector, s, 3, timestamp);
	return EC_SUCCESS;
}

/**
 * Initialise BH1730 Ambient light sensor.
 */
static int sync_init(const struct motion_sensor_t *s)
{
	counter = 0;
	CPRINTF("sync init\n");
	return 0;
}

#ifdef CONFIG_SYNC_COMMAND
static int command_sync(int argc, char **argv)
{
	sync_interrupt(GPIO_SYNC_INT);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(sync, command_sync,
	NULL,
	"Simulates a sync event");
#endif

const struct accelgyro_drv sync_drv = {
	.init = sync_init,
	.read = sync_read,
	.set_data_rate = sync_set_data_rate,
	.get_data_rate = sync_get_data_rate,
	.irq_handler = motion_irq_handler,
	.load_fifo = load_fifo,
};

