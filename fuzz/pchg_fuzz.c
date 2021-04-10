/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test peripheral device charger module.
 */

#define HIDE_EC_STDLIB
#include "common.h"
#include "driver/nfc/ctn730.h"
#include "peripheral_charger.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define TASK_EVENT_FUZZ TASK_EVENT_CUSTOM_BIT(0)

extern struct pchg_drv ctn730_drv;
struct pchg pchgs[] = {
	[0] = {
		.cfg = &(const struct pchg_config) {
			.drv = &ctn730_drv,
			.i2c_port = I2C_PORT_WLC,
			.irq_pin = GPIO_WLC_IRQ_CONN,
			.full_percent = 96,
			.block_size = 128,
		},
		.events = QUEUE_NULL(PCHG_EVENT_QUEUE_SIZE, enum pchg_event),
	},
};
const int pchg_count = ARRAY_SIZE(pchgs);

static pthread_cond_t done_cond;
static pthread_mutex_t lock;

#define MAX_MESSAGES 8
static uint8_t input[MAX_MESSAGES
		     * 256 * sizeof(((struct ctn730_msg*)NULL)->length)];
static uint8_t *head, *tail;
static bool data_available;

int pchg_i2c_xfer(int port, uint16_t addr_flags,
		  const uint8_t *out, int out_size,
		  uint8_t *in, int in_size, int flags)
{
	if (port != I2C_PORT_WLC || addr_flags != CTN730_I2C_ADDR)
		return EC_ERROR_INVAL;

	if (in == NULL || in_size == 0)
		return EC_SUCCESS;

	if (head + in_size >= tail) {
		data_available = false;
		return EC_ERROR_OVERFLOW;
	}

	memcpy(in, head, in_size);
	head += in_size;

	return EC_SUCCESS;
}
DECLARE_TEST_I2C_XFER(pchg_i2c_xfer);

void run_test(int argc, char **argv)
{
	ccprints("Fuzzing task started");
	wait_for_task_started();

	while (1) {
		int i = 0;

		task_wait_event_mask(TASK_EVENT_FUZZ, -1);
		test_chipset_on();
		msleep(1);

		while (data_available && i++ < MAX_MESSAGES) {
			pchg_irq(pchgs[0].cfg->irq_pin);
			msleep(1);
		}
		test_chipset_off();

		pthread_cond_signal(&done_cond);
	}
}

int test_fuzz_one_input(const uint8_t *data, unsigned int size)
{
	if (size < sizeof(struct ctn730_msg))
		return 0;

	head = input;
	tail = input + size;
	memcpy(input, data, size);
	data_available = true;

	task_set_event(TASK_ID_TEST_RUNNER, TASK_EVENT_FUZZ);
	pthread_cond_wait(&done_cond, &lock);

	return 0;
}
