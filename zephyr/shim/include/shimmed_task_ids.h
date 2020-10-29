/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_SHIMMED_TASKS_IDS_H
#define __CROS_EC_SHIMMED_TASKS_IDS_H

/* Include the shimmed tasks for the project/board */
#ifdef CONFIG_SHIMMED_TASKS
#include <shimmed_tasks.h>
#else
#define CROS_EC_TASK_LIST
#endif

/* Define the task_ids globally for all shimmed platform/ec code to use */
#define CROS_EC_TASK(name, ...) _CONCAT(TASK_ID_, name),
enum {
	TASK_ID_IDLE = -1, /* We don't shim the idle task */
	CROS_EC_TASK_LIST
	TASK_ID_COUNT
};
#undef CROS_EC_TASK

#endif /* __CROS_EC_SHIMMED_TASKS_IDS_H */