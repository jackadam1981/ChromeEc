/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#define CONFIG_BC12_STACK_SIZE 800
#define CONFIG_BC12_THREAD_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO

static struct k_event bc12_event;

static void bc12_init(void)
{
	k_event_init(&bc12_event);
}

static void bc12_entry(void)
{
	while (1) {
	}
}

K_THREAD_DEFINE(bc12_tid, CONFIG_BC12_STACK_SIZE, bc12_entry, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);
