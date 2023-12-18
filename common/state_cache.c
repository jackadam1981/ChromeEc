// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "state_cache.h"

#ifdef CONFIG_ZEPHYR
#include <zephyr/kernel.h>
#endif


#ifdef CONFIG_ZEPHYR
void state_cache_dump(void)
{
	const uint8_t *tag;
	uint8_t uint8_tag_count;
	uint8_t uint16_tag_count;
	uint8_t uint32_tag_count;
	uint8_t uint32_ptr_tag_count;
	uint8_t uint64_tag_count;

	TYPE_SECTION_COUNT(uint8_t, state_cache_uint8_t_tag, &uint8_tag_count);
	TYPE_SECTION_COUNT(uint8_t, state_cache_uint16_t_tag, &uint16_tag_count);
	TYPE_SECTION_COUNT(uint8_t, state_cache_uint32_t_tag, &uint32_tag_count);
	TYPE_SECTION_COUNT(uint8_t, state_cache_uint64_t_tag, &uint64_tag_count);
	TYPE_SECTION_COUNT(uint8_t, state_cache_uint32_t_ptr_tag, &uint32_ptr_tag_count);

	printk("uint8_tag_count: %d\n", uint8_tag_count);
	printk("uint16_tag_count: %d\n", uint16_tag_count);
	printk("uint32_tag_count: %d\n", uint32_tag_count);
	printk("uint64_tag_count: %d\n", uint64_tag_count);
	printk("uint32_ptr_tag_count: %d\n", uint32_ptr_tag_count);

	printk("== uint8 state cache ==\n");
	for (int i = 0; i < uint8_tag_count; i++) {
		const uint8_t *val;
		TYPE_SECTION_GET(uint8_t, state_cache_uint8_t_tag, i, &tag);
		TYPE_SECTION_GET(uint8_t, state_cache_uint8_t_val, i, &val);
		printk("%d: %d\n", *tag, *val);
	}
	printk("== uint16 state cache ==\n");
	for (int i = 0; i < uint16_tag_count; i++) {
		const uint16_t *val;
		TYPE_SECTION_GET(uint8_t, state_cache_uint16_t_tag, i, &tag);
		TYPE_SECTION_GET(uint16_t, state_cache_uint16_t_val, i, &val);
		printk("%d: %d\n", *tag, *val);
	}
	printk("== uint32 state cache ==\n");
	for (int i = 0; i < uint32_tag_count; i++) {
		const uint32_t *val;
		TYPE_SECTION_GET(uint8_t, state_cache_uint32_t_tag, i, &tag);
		TYPE_SECTION_GET(uint32_t, state_cache_uint32_t_val, i, &val);
		printk("%d: %d\n", *tag, *val);
	}
	for (int i = 0; i < uint32_ptr_tag_count; i++) {
		const uintptr_t *val;
		TYPE_SECTION_GET(uint8_t, state_cache_uint32_t_ptr_tag, i, &tag);
		TYPE_SECTION_GET(uintptr_t, state_cache_uint32_t_ptr_val, i, &val);
		printk("%d: %d\n", *tag, *((uint32_t *)*val));
	}
	printk("== uint64 state cache ==\n");
	for (int i = 0; i < uint64_tag_count; i++) {
		const uint64_t *val;
		TYPE_SECTION_GET(uint8_t, state_cache_uint64_t_tag, i, &tag);
		TYPE_SECTION_GET(uint64_t, state_cache_uint64_t_val, i, &val);
		printk("%d: %llx\n", *tag, *val);
	}
}
#endif

extern const uint8_t __state_cache_uint8_t_tag[];
extern const uint8_t __state_cache_uint8_t_tag_end[];
extern const uint8_t __state_cache_uint16_t_tag[];
extern const uint8_t __state_cache_uint16_t_tag_end[];
extern const uint8_t __state_cache_uint32_t_tag[];
extern const uint8_t __state_cache_uint32_t_tag_end[];
extern const uint8_t __state_cache_uint64_t_tag[];
extern const uint8_t __state_cache_uint64_t_tag_end[];

extern const uint8_t __state_cache_uint8_t_val[];
extern const uint16_t __state_cache_uint16_t_val[];
extern const uint32_t __state_cache_uint32_t_val[];
extern const uint64_t __state_cache_uint64_t_val[];

void state_cache_dump(void)
{
	const uint8_t uint8_tag_count = __state_cache_uint8_t_tag_end - __state_cache_uint8_t_tag;
	const uint8_t uint16_tag_count = __state_cache_uint16_t_tag_end - __state_cache_uint16_t_tag;
	const uint8_t uint32_tag_count = __state_cache_uint32_t_tag_end - __state_cache_uint32_t_tag;
	const uint8_t uint64_tag_count = __state_cache_uint64_t_tag_end - __state_cache_uint64_t_tag;

	ccprintf("state_cache_dump\n");

	ccprintf("uint8_tag_count: %d\n", uint8_tag_count);
	ccprintf("uint16_tag_count: %d\n", uint16_tag_count);
	ccprintf("uint32_tag_count: %d\n", uint32_tag_count);
	ccprintf("uint64_tag_count: %d\n", uint64_tag_count);

	ccprintf("== uint8 state cache ==\n");
	for (int i = 0; i < uint8_tag_count; i++) {
		uint8_t tag = __state_cache_uint8_t_tag[i];
		uint8_t val = __state_cache_uint8_t_val[i];
		ccprintf("%d: %x\n", tag, val);
	}
	ccprintf("== uint16 state cache ==\n");
	for (int i = 0; i < uint16_tag_count; i++) {
		uint8_t tag = __state_cache_uint16_t_tag[i];
		uint16_t val = __state_cache_uint16_t_val[i];
		ccprintf("%d: %x\n", tag, val);
	}
	ccprintf("== uint32 state cache ==\n");
	for (int i = 0; i < uint32_tag_count; i++) {
		uint8_t tag = __state_cache_uint32_t_tag[i];
		uint32_t val = __state_cache_uint32_t_val[i];
		ccprintf("%d: %x\n", tag, val);
	}
	ccprintf("== uint64 state cache ==\n");
	for (int i = 0; i < uint64_tag_count; i++) {
		uint8_t tag = __state_cache_uint64_t_tag[i];
		uint64_t val = __state_cache_uint64_t_val[i];
		ccprintf("%d: %llx\n", tag, val);
	}
}

static int command_state_cache(int argc, const char **argv)
{
	// uint8_t buffer[256];
	state_cache_dump();
	// state_cache_pack(buffer, 256);
	// state_cache_dump_packed(buffer, 256);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(sc, command_state_cache, "", "");
