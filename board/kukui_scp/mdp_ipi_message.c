/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2018. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 */

/*
============================================================================================================
------------------------------------------------------------------------------------------------------------
||                        Header Files
------------------------------------------------------------------------------------------------------------
============================================================================================================
*/
#include <console.h>
#include <hooks.h>
#include <queue.h>
#include <queue_policies.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <task.h>
#include <util.h>

#include "chip/mt_scp/ipi_chip.h"
#include "chip/mt_scp/registers.h"
#include "mdp_ipi_message.h"

#define CPRINTF(format, args...) cprintf(CC_IPI, format, ##args)
#define CPRINTS(format, args...) cprints(CC_IPI, format, ##args)

/* Forwad declaration. */
static struct consumer const event_mdp_consumer;
static void event_mdp_written(struct consumer const *consumer, size_t count);

static struct queue const event_mdp_queue = QUEUE_DIRECT(4,
	struct mdp_msg_service, null_producer, event_mdp_consumer);
static struct consumer const event_mdp_consumer = {
	.queue = &event_mdp_queue,
	.ops = &((struct consumer_ops const) {
		.written = event_mdp_written,
	}),
};

/* Stub functions only provided by private overlays. */
#ifndef HAVE_PRIVATE_MT8183
void mdp_common_init(void) {}
void mdp_ipi_task_handler(void *pvParameters) {}
#endif

static struct mutex mdp_lock;

static void event_mdp_written(struct consumer const *consumer, size_t count)
{
	task_wake(TASK_ID_MDP_SERVICE);
}

/*
============================================================================================================
------------------------------------------------------------------------------------------------------------
||                        IPI Handler
------------------------------------------------------------------------------------------------------------
============================================================================================================
*/
static void mdp_ipi_handler(int id, void *data, unsigned int len)
{
	struct mdp_msg_service cmd;

	cmd.id = id;
	memcpy(cmd.msg, data, MIN(len, sizeof(cmd.msg)));

	/*
	 * If there is no other IPI handler touch this queue, we don't need to
	 * interrupt_disable() or task_disable_irq().
	 */
	mutex_lock(&mdp_lock);
	if (!queue_add_unit(&event_mdp_queue, &cmd))
		CPRINTS("Could not send mdp id: %d to the queue.", id);
	mutex_unlock(&mdp_lock);
}
DECLARE_IPI(IPI_MDP_INIT, mdp_ipi_handler, 1);
DECLARE_IPI(IPI_MDP_FRAME, mdp_ipi_handler, 1);
DECLARE_IPI(IPI_MDP_DEINIT, mdp_ipi_handler, 1);

/* This function renames from mdp_service_entry. */
void mdp_service_task(void *u)
{
	struct mdp_msg_service rsv_msg;
	size_t size;

	mdp_common_init();

	while (1) {
		/*
		 * Queue unit is added in IPI handler, which is in ISR context.
		 * Disable IRQ to prevent a clobbered queue.
		 */
		task_disable_irq(SCP_IRQ_IPC0);
		mutex_lock(&mdp_lock);
		size = queue_remove_unit(&event_mdp_queue, &rsv_msg);
		mutex_unlock(&mdp_lock);
		task_enable_irq(SCP_IRQ_IPC0);

		if (!size)
			task_wait_event(-1);
		else
			mdp_ipi_task_handler(&rsv_msg);
	}
}
