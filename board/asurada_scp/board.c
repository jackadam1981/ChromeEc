/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Asurada SCP configuration */

#include "registers.h"

#include "gpio_list.h"

#include "ipi_chip.h"
#include "queue.h"
#include "queue_policies.h"
#include "task.h"
#include "timer.h"

struct q_msg {
	int32_t ipi_id;
};

static struct consumer const q_consumer;

static void q_written(struct consumer const *consumer, size_t count)
{
	task_wake(TASK_ID_X);
}

static struct queue const q = QUEUE_DIRECT(8, struct q_msg, null_producer, q_consumer);
static struct consumer const q_consumer = {
	.queue = &q,
	.ops = &((struct consumer_ops const) {
			.written = q_written,
		}),
};

static void x(int32_t id, void *data, uint32_t len)
{
	struct q_msg qmsg;

	if (!len)
		return;

	qmsg.ipi_id = id;

	if (!queue_add_unit(&q, &qmsg))
		ccprints("failed to queue_add_unit");
}
DECLARE_IPI(10, x, 0);

void x_task(void *u)
{
	struct q_msg qmsg;
	size_t size;

	while (1) {
		ipi_disable_irq();
		size = queue_remove_unit(&q, &qmsg);
		ipi_enable_irq();

		if (!size)
			task_wait_event(-1);
		else
			ipi_send(qmsg.ipi_id, 0, 0, 0);
	}
}
