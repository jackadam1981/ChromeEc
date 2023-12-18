/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "state_cache.h"

struct uint8_entry {
	uint8_t tag;
	uint8_t val;
} __packed;

struct uint16_entry {
	uint8_t tag;
	uint16_t val;
} __packed;

struct uint32_entry {
	uint8_t tag;
	uint32_t val;
} __packed;

struct uint64_entry {
	uint8_t tag;
	uint64_t val;
} __packed;

#ifdef CONFIG_ZEPHYR

#include <zephyr/kernel.h>

#define STATE_CACHE_GET_COUNT(type, dst) \
	TYPE_SECTION_COUNT(uint8_t, state_cache_##type##_tag, dst)

#define STATE_CACHE_GET_TAG_VAL(type, idx, tag, val)                     \
	do {                                                             \
		const uint8_t *tag_tmp;                                  \
		const type *val_tmp;                                     \
		TYPE_SECTION_GET(uint8_t, state_cache_##type##_tag, idx, \
				 &tag_tmp);                              \
		TYPE_SECTION_GET(type, state_cache_##type##_val, idx,    \
				 &val_tmp);                              \
		*(tag) = *tag_tmp;                                       \
		*(val) = *val_tmp;                                       \
	} while (0)

#else

#define STATE_CACHE_GET_COUNT(type, dst)                               \
	do {                                                           \
		extern const uint8_t __state_cache_##type##_tag[];     \
		extern const uint8_t __state_cache_##type##_tag_end[]; \
		*(dst) = (__state_cache_##type##_tag_end -             \
			  __state_cache_##type##_tag);                 \
	} while (0)

#define STATE_CACHE_GET_TAG_VAL(type, idx, tag, val)               \
	do {                                                       \
		extern const uint8_t __state_cache_##type##_tag[]; \
		extern const type __state_cache_##type##_val[];    \
		*(tag) = __state_cache_##type##_tag[i];            \
		*(val) = __state_cache_##type##_val[i];            \
	} while (0)

#endif /* CONFIG_ZEPHYR */

void state_cache_dump(void)
{
	uint8_t tag;
	uint8_t uint8_count;
	uint8_t uint16_count;
	uint8_t uint32_count;
	uint8_t uint64_count;

	ccprintf("** STATE CACHE **\n");

	STATE_CACHE_GET_COUNT(uint8_t, &uint8_count);
	STATE_CACHE_GET_COUNT(uint16_t, &uint16_count);
	STATE_CACHE_GET_COUNT(uint32_t, &uint32_count);
	STATE_CACHE_GET_COUNT(uint64_t, &uint64_count);

	ccprintf("== uint8 (%d) ==\n", uint8_count);
	for (int i = 0; i < uint8_count; i++) {
		uint8_t val;

		STATE_CACHE_GET_TAG_VAL(uint8_t, i, &tag, &val);
		ccprintf("%d: %x\n", tag, val);
	}
	ccprintf("== uint16 (%d) ==\n", uint16_count);
	for (int i = 0; i < uint16_count; i++) {
		uint16_t val;

		STATE_CACHE_GET_TAG_VAL(uint16_t, i, &tag, &val);
		ccprintf("%d: %x\n", tag, val);
	}
	ccprintf("== uint32 (%d) ==\n", uint32_count);
	for (int i = 0; i < uint32_count; i++) {
		uint32_t val;

		STATE_CACHE_GET_TAG_VAL(uint32_t, i, &tag, &val);
		ccprintf("%d: %x\n", tag, val);
	}
	ccprintf("== uint64 (%d) ==\n", uint64_count);
	for (int i = 0; i < uint64_count; i++) {
		uint64_t val;

		STATE_CACHE_GET_TAG_VAL(uint64_t, i, &tag, &val);
		ccprintf("%d: %llx\n", tag, val);
	}
}

int state_cache_pack(uint8_t *buf, int buf_size)
{
	struct state_cache_packed_header *header = (void *)buf;
	uint8_t *body = buf + sizeof(*header);
	int total_size;
	uint8_t uint8_count;
	uint8_t uint16_count;
	uint8_t uint32_count;
	uint8_t uint64_count;

	STATE_CACHE_GET_COUNT(uint8_t, &uint8_count);
	STATE_CACHE_GET_COUNT(uint16_t, &uint16_count);
	STATE_CACHE_GET_COUNT(uint32_t, &uint32_count);
	STATE_CACHE_GET_COUNT(uint64_t, &uint64_count);

	total_size = sizeof(struct state_cache_packed_header) +
		     sizeof(struct uint8_entry) * uint8_count +
		     sizeof(struct uint16_entry) * uint16_count +
		     sizeof(struct uint32_entry) * uint32_count +
		     sizeof(struct uint64_entry) * uint64_count;

	if (buf_size < total_size) {
		ccprintf("%d byte buffer cannot fit %d byte state cache\n",
			 buf_size, total_size);
		return -1;
	}

	header->version = 1;
	header->uint8_count = uint8_count;
	header->uint16_count = uint16_count;
	header->uint32_count = uint32_count;
	header->uint64_count = uint64_count;

	struct uint8_entry *uint8_entries = (struct uint8_entry *)(body);
	struct uint16_entry *uint16_entries =
		(struct uint16_entry *)(uint8_entries +
					sizeof(struct uint8_entry) *
						header->uint8_count);
	struct uint32_entry *uint32_entries =
		(struct uint32_entry *)(uint16_entries +
					sizeof(struct uint16_entry) *
						header->uint16_count);
	struct uint64_entry *uint64_entries =
		(struct uint64_entry *)(uint32_entries +
					sizeof(struct uint32_entry) *
						header->uint32_count);

	for (int i = 0; i < header->uint8_count; i++)
		STATE_CACHE_GET_TAG_VAL(uint8_t, i, &(uint8_entries[i].tag),
					&(uint8_entries[i].val));

	for (int i = 0; i < header->uint16_count; i++)
		STATE_CACHE_GET_TAG_VAL(uint16_t, i, &(uint16_entries[i].tag),
					&(uint16_entries[i].val));

	for (int i = 0; i < header->uint32_count; i++)
		STATE_CACHE_GET_TAG_VAL(uint32_t, i, &(uint32_entries[i].tag),
					&(uint32_entries[i].val));

	for (int i = 0; i < header->uint64_count; i++)
		STATE_CACHE_GET_TAG_VAL(uint64_t, i, &(uint64_entries[i].tag),
					&(uint64_entries[i].val));

	return total_size;
}

int state_cache_dump_packed(uint8_t *buf, int buf_size)
{
	int total_size;
	struct state_cache_packed_header *header = (void *)buf;
	uint8_t *body = buf + sizeof(*header);

	struct uint8_entry *uint8_entries = (struct uint8_entry *)(body);
	struct uint16_entry *uint16_entries =
		(struct uint16_entry *)(uint8_entries +
					sizeof(struct uint8_entry) *
						header->uint8_count);
	struct uint32_entry *uint32_entries =
		(struct uint32_entry *)(uint16_entries +
					sizeof(struct uint16_entry) *
						header->uint16_count);
	struct uint64_entry *uint64_entries =
		(struct uint64_entry *)(uint32_entries +
					sizeof(struct uint32_entry) *
						header->uint32_count);

	if (buf_size < sizeof(struct state_cache_packed_header)) {
		ccprintf("packed state cache buffer size (%d) is invalid\n",
			 buf_size);
		return -1;
	}

	if (header->version != 1) {
		ccprintf("packed state cache version (%d) is not supported",
			 header->version);
		return -1;
	}

	total_size = sizeof(struct state_cache_packed_header) +
		     sizeof(struct uint8_entry) * header->uint8_count +
		     sizeof(struct uint16_entry) * header->uint16_count +
		     sizeof(struct uint32_entry) * header->uint32_count +
		     sizeof(struct uint64_entry) * header->uint64_count;

	if (buf_size < total_size) {
		ccprintf("buffer size (%d) does not fit state cache (%d)\n",
			 buf_size, total_size);
		return -1;
	}

	ccprintf("packed state cache version = %d\n", header->version);

	ccprintf("== uint8 (%d) ==\n", header->uint8_count);
	for (int i = 0; i < header->uint8_count; i++)
		ccprintf("%d: %x\n", uint8_entries[i].tag,
			 uint8_entries[i].val);

	ccprintf("== uint16 (%d) ==\n", header->uint16_count);
	for (int i = 0; i < header->uint16_count; i++)
		ccprintf("%d: %x\n", uint16_entries[i].tag,
			 uint16_entries[i].val);

	ccprintf("== uint32 (%d) ==\n", header->uint32_count);
	for (int i = 0; i < header->uint32_count; i++)
		ccprintf("%d: %x\n", uint32_entries[i].tag,
			 uint32_entries[i].val);

	ccprintf("== uint64 (%d) ==\n", header->uint64_count);
	for (int i = 0; i < header->uint64_count; i++)
		ccprintf("%d: %llx\n", uint64_entries[i].tag,
			 uint64_entries[i].val);

	return total_size;
}

static int command_state_cache(int argc, const char **argv)
{
	uint8_t buf[256];
	int size;

	state_cache_dump();

	size = state_cache_pack(buf, 256);
	ccprintf("Packed size = %d\n", size);
	size = state_cache_dump_packed(buf, 256);
	ccprintf("Unpacked size = %d\n", size);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(state_cache, command_state_cache, "",
			"Dump State Cache");
