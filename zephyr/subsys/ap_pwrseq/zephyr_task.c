/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <init.h>
#include <zephyr/types.h>
#include <x86_non_dsx_common_pwrseq_sm_handler.h>

static K_KERNEL_STACK_DEFINE(pwrseq_thread_stack,
			CONFIG_X86_NON_DSW_PWRSEQ_STACK_SIZE);
static struct k_thread pwrseq_thread_data;

/*
 * Task thread.
 */
static void pwrseq_task_start(void *p1, void *p2, void *p3)
{
	/* Call entry point */
	ap_pwrseq_task_entry();
}

/*
 * Task entry point for AP power sequence.
 * Initialise the system state, then
 * enter the state machine loop.
 */
void pwrseq_init(void)
{
	k_thread_create(&pwrseq_thread_data,
			pwrseq_thread_stack,
			K_KERNEL_STACK_SIZEOF(pwrseq_thread_stack),
			(k_thread_entry_t)pwrseq_task_start,
			NULL, NULL, NULL,
			K_PRIO_COOP(8), 0, K_NO_WAIT);

	k_thread_name_set(&pwrseq_thread_data, "pwrseq_task");
}

SYS_INIT(pwrseq_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
