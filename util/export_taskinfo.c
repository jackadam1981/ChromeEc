/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <stdlib.h>

#include "config.h"
#include "export_taskinfo.h"
#include "task_id.h"

#ifdef SECTION_IS_RO
#define GET_TASKINFOS_FUNC get_ro_taskinfos
#else /* SECTION_IS_RW */
#define GET_TASKINFOS_FUNC get_rw_taskinfos
#endif

/**
 * TODO(cheyuw): Workaround for host board since it doesn't define
 * TASK_STACK_SIZE but uses it in the tasklist. (crbug.com/752923).
 */
#ifdef BOARD_HOST
#define TASK_STACK_SIZE 0
#endif

#define TASK(n, r, d, s)  {	\
	.name = #n,		\
	.routine = #r,		\
	.stack_size = s,	\
},
static const struct taskinfo const taskinfos[] = {
	CONFIG_TASK_LIST
	CONFIG_TEST_TASK_LIST
	CONFIG_CTS_TASK_LIST
};
#undef TASK

uint32_t GET_TASKINFOS_FUNC(const struct taskinfo **infos)
{
	*infos = taskinfos;
	/* Calculate the number of tasks */
	return sizeof(taskinfos) / sizeof(*taskinfos);
}
