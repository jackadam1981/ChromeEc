/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <dma.h>
#include <string.h>

#include "isp_p1_srv.h"

#include "chip/mt_scp/ipi_chip.h"
#include "chip/mt_scp/registers.h"
#include "queue_policies.h"
#include "console.h"
#include "hooks.h"
#include "task.h"
#include "util.h"
#include "queue.h"

#define CPRINTF(format, args...) cprintf(CC_IPI, format, ##args)
#define CPRINTS(format, args...) cprints(CC_IPI, format, ##args)

static struct mutex isp_lock;

/* Forwad declaration. */
static struct consumer const event_isp_consumer;
static void event_isp_written(struct consumer const *consumer, size_t count);

static struct queue const event_isp_queue = QUEUE_DIRECT(6,
	struct isp_msg_service, null_producer, event_isp_consumer);

static struct consumer const event_isp_consumer = {
	.queue = &event_isp_queue,
	.ops = &((struct consumer_ops const) {
		.written = event_isp_written,
	}),
};

/* Stub functions only provided by private overlays. */
#ifndef HAVE_PRIVATE_MT8183
void isp_msg_handler(void *data) {}
#endif

static void event_isp_written(struct consumer const *consumer, size_t count)
{
	task_wake(TASK_ID_ISP_SERVICE);
}

static void isp_ipi_cmd_handler(int id, void *data, uint32_t len)
{
	struct isp_msg_service rsv_msg;

	if (!len)
		return;

	rsv_msg.id = ISP_CMD;
	memcpy(rsv_msg.msg, data, MIN(len, sizeof(rsv_msg.msg)));

	/*
	 * If there is no other IPI handler touch this queue, we don't need to
	 * interrupt_disable() or task_disable_irq().
	 */
	mutex_lock(&isp_lock);
	if (!queue_add_unit(&event_isp_queue, &rsv_msg))
		CPRINTS("Could not send isp %d to the queue.", id);
	mutex_unlock(&isp_lock);
}
DECLARE_IPI(IPI_ISP_CMD, isp_ipi_cmd_handler, 1);

static void isp_ipi_frame_handler(int id, void *data, uint32_t len)
{
	struct isp_msg_service rsv_msg;

	if (!len)
		return;

	rsv_msg.id = ISP_FRAME;
	memcpy(rsv_msg.msg, data, MIN(len, sizeof(rsv_msg.msg)));

	/*
	 * If there is no other IPI handler touch this queue, we don't need to
	 * interrupt_disable() or task_disable_irq().
	 */
	mutex_lock(&isp_lock);
	if (!queue_add_unit(&event_isp_queue, &rsv_msg))
		CPRINTS("Could not send isp %d to the queue.", id);
	mutex_unlock(&isp_lock);
}
DECLARE_IPI(IPI_ISP_FRAME, isp_ipi_frame_handler, 1);

/* This function renames from isp_service_entry. */
void isp_service_task(void *u)
{
	struct isp_msg_service rsv_msg;
	size_t size;

	while (1) {
		/*
		 * Queue unit is added in IPI handler, which is in ISR context.
		 * Disable IRQ to prevent a clobbered queue.
		 */
		task_disable_irq(SCP_IRQ_IPC0);
		mutex_lock(&isp_lock);
		size = queue_remove_unit(&event_isp_queue, &rsv_msg);
		mutex_unlock(&isp_lock);
		task_enable_irq(SCP_IRQ_IPC0);

		if (!size)
			task_wait_event(-1);
		else
			isp_msg_handler(&rsv_msg);
	}
}

