/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <dma.h>
#include <string.h>

#include "isp_p2_srv.h"
#include "ec_wrapper.h"

#include "chip/mt_scp/ipi_chip.h"
#include "chip/mt_scp/registers.h"
#include "queue_policies.h"
#include "console.h"
#include "hooks.h"
#include "task.h"
#include "util.h"
#include "queue.h"

static struct mutex dip_lock;

/* Forwad declaration. */
static struct consumer const event_dip_consumer;
static void event_dip_written(struct consumer const *consumer, size_t count);

static struct queue const event_dip_queue = QUEUE_DIRECT(3,
	struct dip_msg_service, null_producer, event_dip_consumer);

static struct consumer const event_dip_consumer = {
	.queue = &event_dip_queue,
	.ops = &((struct consumer_ops const) {
		.written = event_dip_written,
	}),
};

/* Stub functions only provided by private overlays. */
#ifndef HAVE_PRIVATE_MT8183
void dip_msg_handler(void *data) {}
#endif

static void event_dip_written(struct consumer const *consumer, size_t count)
{
	task_wake(TASK_ID_DIP_SERVICE);
}

static void dip_scp_ipi_handler(int id, void *data, uint32_t len)
{
	struct dip_msg_service rsv_msg;
    PRINTF_E("20190424[walter]===public,dip_scp_ipi_handler=====\n");

	if (!len)
		return;
    PRINTF_E("[walter]+++++++++++++++++++++++++\n");
	rsv_msg.id = DIP_CMD;
	memcpy(rsv_msg.msg, data, MIN(len, sizeof(rsv_msg.msg)));

    PRINTF_E("[walter]ipi,rsv_msg.id = %d\n",rsv_msg.id);
    PRINTF_E("[walter]ipi,rsv_msg.msg = \n");
    for(int i = 0; i < 10;i++)
        PRINTF_E("[%d] = 0x%x, ",i,rsv_msg.msg[i]);
    PRINTF_E("\n");

	/*
	 * If there is no other IPI handler touch this queue, we don't need to
	 * interrupt_disable() or task_disable_irq().
	 */
	mutex_lock(&dip_lock);
	if (!queue_add_unit(&event_dip_queue, &rsv_msg))
		CPRINTS("Could not send dip %d to the queue.\n", id);
	mutex_unlock(&dip_lock);

    PRINTF_E("[walter]----------------------------\n");
}
DECLARE_IPI(IPI_DIP, dip_scp_ipi_handler, 1);

/* This function renames from dip_service_entry. */
void dip_service_task(void *u)
{
	struct dip_msg_service rsv_msg;
	size_t size;

    PRINTF_E("//20190424[walter]===public,dip_service_task=====\n");

	while (1) {
		/*
		 * Queue unit is added in IPI handler, which is in ISR context.
		 * Disable IRQ to prevent a clobbered queue.
		 */
		task_disable_irq(SCP_IRQ_IPC0);
		mutex_lock(&dip_lock);
		size = queue_remove_unit(&event_dip_queue, &rsv_msg);
        mutex_unlock(&dip_lock);
		task_enable_irq(SCP_IRQ_IPC0);

        PRINTF_E("[walter]task,rsv_msg.id = %d\n",rsv_msg.id);
        PRINTF_E("[walter]task,rsv_msg.msg = \n");
        for(int i = 0; i < 10;i++)
            PRINTF_E("[%d] = 0x%x, ",i,rsv_msg.msg[i]);
        PRINTF_E("\n");

		if (!size)
        {
            PRINTF_E("[walter](!size)\n");
            task_wait_event(-1);
        }
		else
        {
            PRINTF_E("[walter]dip_msg_handler\n");
			dip_msg_handler(&rsv_msg);
        }
	}
}
