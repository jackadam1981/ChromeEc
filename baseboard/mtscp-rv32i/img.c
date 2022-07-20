/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "dma.h"
#include "ipi_chip.h"
#include "img.h"
#include "queue_policies.h"
#include "registers.h"
#include "task.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)

/* Forwad declaration. */
static struct consumer const event_img_consumer;
static void event_img_written(struct consumer const *consumer, size_t count);

static struct queue const event_img_queue = QUEUE_DIRECT(8,
	struct img_msg, null_producer, event_img_consumer);

static struct consumer const event_img_consumer = {
	.queue = &event_img_queue,
	.ops = &((struct consumer_ops const) {
		.written = event_img_written,
	}),
};

/* Stub functions only provided by private overlays. */
#ifndef HAVE_PRIVATE_MT_SCP_CORE1
void ipi_img_handler(void *data) {}
#endif

static void event_img_written(struct consumer const *consumer, size_t count)
{
	task_wake(TASK_ID_IMG_SERVICE);
}

static void img_ipi_handler(int id, void *data, uint32_t len)
{
	struct img_msg rsv_msg;
	CPRINTS("Get IMGSYS cmd %d, len %d", id, len);

	if (!len)
		return;

	rsv_msg.id = id;
	memcpy(rsv_msg.msg, data, MIN(len, sizeof(rsv_msg.msg)));

	/*
	 * If there is no other IPI handler touch this queue, we don't need to
	 * interrupt_disable() or task_disable_irq().
	 */
	if (!queue_add_unit(&event_img_queue, &rsv_msg))
		CPRINTS("Failed to send img %d to the queue", id);
}
DECLARE_IPI(SCP_IPI_IMG_INIT, img_ipi_handler, 0);
DECLARE_IPI(SCP_IPI_IMG_FRAME, img_ipi_handler, 0);
DECLARE_IPI(SCP_IPI_IMG_CLR, img_ipi_handler, 0);

/* This function renames from cam_service_entry. */
void img_service_task(void *u)
{
	struct img_msg rsv_msg;
	size_t size;

	while (1) {
		/*
		 * Queue unit is added in IPI handler, which is in ISR context.
		 * Disable IRQ to prevent a clobbered queue.
		 */
		ipi_disable_irq();
		size = queue_remove_unit(&event_img_queue, &rsv_msg);
		ipi_enable_irq();

		if (!size)
			task_wait_event(-1);
		else
			imgsys_msg_handler(&rsv_msg, rsv_msg.id);
	}
}
