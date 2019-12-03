/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stddef.h>
#include <string.h>

#include "common.h"
#include "hooks.h"
#include "system.h"

char preserved_region[CONFIG_PRESERVED_REGION_SIZE]
	__attribute__((section("preserved_region")));

#define PRESERVED_REGION_SYSJUMP_TAG 0x5052 /* "PR" */
#define PRESERVED_REGION_SYSJUMP_VERSION 1

struct preserved_region_tag {
	size_t size;
};



#ifdef CONFIG_PRESERVED_REGION_JUMPTAG

static int preserved_region_add_tagvoid)
{
	struct preserved_region_tag tag;
	memset(&tag, 0, sizeof(tag));
	tag.size = sizeof(preserved_region);

	system_add_jump_tag(PRESERVED_REGION_SYSJUMP_TAG,
			    PRESERVED_REGION_SYSJUMP_VERSION,
			    sizeof(tag), &tag);
	return EC_SUCCESS;
}
DECLARE_HOOK(HOOK_SYSJUMP, preserved_region_add_tagvoid, HOOK_PRIO_DEFAULT);

#endif /* CONFIG_PRESERVED_REGION_JUMPTAG */

/*
 *
 */
// BUILD_ASSERT((CONFIG_PRESERVED_DATA_SIZE % sizeof(void *)) == 0);