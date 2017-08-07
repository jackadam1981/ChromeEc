/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __UTIL_EXPORT_TASKINFO_H
#define __UTIL_EXPORT_TASKINFO_H

#include <stdint.h>

struct taskinfo {
	char *name;
	char *routine;
	uint32_t stack_size;
};

/**
 * These two accessors are implemented in export_taskinfo.c with different
 * build options to get tasklists for different sections.
 */
uint32_t get_ro_taskinfos(const struct taskinfo **infos);
uint32_t get_rw_taskinfos(const struct taskinfo **infos);

#endif /* __UTIL_EXPORT_TASKINFO_H */
