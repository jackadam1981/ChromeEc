/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @brief Shared mem implementation that uses malloc/free from libc.
 */

#include "common.h"
#include "console.h"
#include "link_defs.h"
#include "shared_mem.h"
#include "system.h"
#include "task.h"

#include <stdlib.h>

#include <malloc.h>
#include "fpc_malloc.h"
#include "fpc_static_malloc.h"

int shared_mem_size(void)
{
	return system_usable_ram_end() - (uintptr_t)__shared_mem_buf;
}

static bool is_fpc_static_setup = false;

static void fpc_static_setup_init(void)
{
	int buffer_size = 0x40000;

	void *static_buffer = malloc(buffer_size);

	if (static_buffer) {
		fpc_static_malloc_setup(static_buffer, buffer_size);
		is_fpc_static_setup = true;
		ccprintf("fpc_static_malloc_setup success\n");
	} else {
		ccprintf("fpc_static_malloc_setup failed\n");
	}
}

int shared_mem_acquire(int size, char **dest_ptr)
{
	*dest_ptr = NULL;

	if (in_interrupt_context())
		return EC_ERROR_INVAL;

	if (!is_fpc_static_setup) fpc_static_setup_init();

	*dest_ptr = fpc_malloc(size);
	if (!*dest_ptr)
		return EC_ERROR_BUSY;

	return EC_SUCCESS;
}

void shared_mem_release(void *ptr)
{
	if (in_interrupt_context())
		return;

	fpc_free(ptr);
}

#ifdef CONFIG_CMD_SHMEM
static int command_shmem(int argc, const char **argv)
{
	struct mallinfo info = mallinfo();

	ccprintf("Total:         %d\n", shared_mem_size());
	ccprintf("Allocated:     %d\n", info.uordblks);
	ccprintf("Free:          %d\n", info.fordblks + info.fsmblks);

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(shmem, command_shmem, NULL,
			     "Print shared memory stats");
#endif /* CONFIG_CMD_SHMEM */
