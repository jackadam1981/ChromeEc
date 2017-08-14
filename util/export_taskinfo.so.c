/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This file will be compiled in three places: cmd_c_to_taskinfo with RO and RW,
 * and cmd_c_to_build.
 *
 * The cmd_c_to_taskinfo will compile this file with different
 * section definitions to export different tasklists.
 *
 * It is only for dependency in cmd_c_to_build, so we leave
 * this file empty when compiling without -DEXPORT_TASKINFO.
 */

#ifdef EXPORT_TASKINFO

#include <stdint.h>

#include "config.h"
#include "task_id.h"

#ifdef SECTION_IS_RO
#define GET_TASKINFOS_FUNC get_ro_taskinfos
#else /* SECTION_IS_RW */
#define GET_TASKINFOS_FUNC get_rw_taskinfos
#endif

struct taskinfo {
	char *name;
	char *routine;
	uint32_t stack_size;
};

#define TASK(n, r, d, s)  {	\
	.name = #n,		\
	.routine = #r,		\
	.stack_size = s,	\
},
static const struct taskinfo const taskinfos[] = {
	CONFIG_TASK_LIST
};
#undef TASK

uint32_t GET_TASKINFOS_FUNC(const struct taskinfo **infos)
{
	*infos = taskinfos;
	/* Calculate the number of tasks */
	return sizeof(taskinfos) / sizeof(*taskinfos);
}

#endif
