/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config_chip.h"
#include "pdc_config.h"
#include "pdc_thread.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pdc_thread, CONFIG_USBC_LOG_LEVEL);

struct pdc_msg {
	const struct device *dev;
	uint32_t event;
};

#define MSGQ_LEN (CONFIG_USB_PD_PORT_MAX_COUNT * 2)
K_MSGQ_DEFINE(kmsgq, sizeof(struct pdc_msg), MSGQ_LEN, 4);

static atomic_t thread_started = ATOMIC_INIT(0);
static atomic_t idle = ATOMIC_INIT(0);
static k_tid_t thread;
static struct k_thread thread_data;
K_THREAD_STACK_DEFINE(pdc_thread_stack_area,
		      CONFIG_USBC_PDC_SINGLE_THREAD_STACK_SIZE);

int pdc_thread_post_msg_(const struct device *dev, uint32_t event,
			 const char *caller)
{
	struct pdc_msg msg = { .dev = dev, .event = event };

	LOG_INF("Q[%d/%d] Posting Msg [%s][0x%x] - %s",
		k_msgq_num_used_get(&kmsgq), MSGQ_LEN,
		(msg.dev->name ? msg.dev->name : "NULL"), msg.event, caller);
	return k_msgq_put(&kmsgq, &msg, K_NO_WAIT);
}

bool pdc_thread_is_idle()
{
	return (atomic_get(&idle) != 0 && k_msgq_num_used_get(&kmsgq) == 0);
}

int pdc_thread_wait_for_idle()
{
	int cnt = 0;
	while (!pdc_thread_is_idle() && cnt++ < 20) {
		k_msleep(50);
	}
	return cnt < 20 ? 0 : -ETIMEDOUT;
}

static void pdc_thread_idle(struct k_work *work)
{
	LOG_INF("idle");
	atomic_set(&idle, 1);
}

K_WORK_DELAYABLE_DEFINE(pdc_thread_work, pdc_thread_idle);

static void pdc_thread(void *unused, void *unused1, void *unused2)
{
	struct pdc_msg msg;
	const struct pdc_config_t *cfg;

	while (1) {
		atomic_set(&idle, 0);
		k_work_reschedule(&pdc_thread_work, K_MSEC(100));
		k_msgq_get(&kmsgq, &msg, K_FOREVER);

		cfg = msg.dev->config;
		LOG_INF("Q[%d/%d] Process [%s][0x%x]",
			k_msgq_num_used_get(&kmsgq), MSGQ_LEN,
			(msg.dev->name ? msg.dev->name : "NULL"), msg.event);
		cfg->process_event(msg.dev, msg.event);
	}
}

void pdc_thread_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	if (!atomic_test_and_set_bit(&thread_started, 0)) {
		thread = k_thread_create(
			&thread_data, pdc_thread_stack_area,
			K_THREAD_STACK_SIZEOF(pdc_thread_stack_area),
			pdc_thread, 0, 0, 0,
			CONFIG_USBC_PDC_SINGLE_THREAD_PRIORITY, K_ESSENTIAL,
			K_NO_WAIT);
	}
}
