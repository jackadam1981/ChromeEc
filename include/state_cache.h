/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STATE_CACHE_H
#define __CROS_EC_STATE_CACHE_H


#include "state_cache_tags.h"
#include "common.h"

#define CONFIG_STATE_CACHE_TAG_NAME

#define STATE_CACHE_VERSION 1

struct state_cache_packed_header {
	uint8_t version;

	uint8_t uint8_tag_count;
	uint8_t uint16_tag_count;
	uint8_t uint32_tag_count;
	uint8_t uint64_tag_count;
};

#ifdef CONFIG_STATE_CACHE_TAG_NAME
#define _STATE_CACHE_SET_NAME(_tag) \
		.name = #_tag,
#else
#define _STATE_CACHE_SET_NAME(_tag)
#endif

#define _STATE_CACHE_FULL_TAG_uint8_t(_tag) STATE_CACHE_UINT8_TAG_##_tag
#define _STATE_CACHE_FULL_TAG_uint16_t(_tag) STATE_CACHE_UINT16_TAG_##_tag
#define _STATE_CACHE_FULL_TAG_uint32_t(_tag) STATE_CACHE_UINT32_TAG_##_tag
#define _STATE_CACHE_FULL_TAG_uint64_t(_tag) STATE_CACHE_UINT64_TAG_##_tag

#ifdef CONFIG_ZEPHYR
#define STATE_CACHE_BIND(tag, type, varname) \
	const static TYPE_SECTION_ITERABLE(uint8_t, varname##_##tag, state_cache_##type##_tag, varname) = _STATE_CACHE_FULL_TAG_##type(tag); \
	static TYPE_SECTION_ITERABLE(type, varname, state_cache_##type##_val, varname)

#define STATE_CACHE_BIND_PTR(tag, type, varname) \
	const static TYPE_SECTION_ITERABLE(uint8_t, varname##_##tag, state_cache_##type##_ptr_tag, varname) = _STATE_CACHE_FULL_TAG_##type(tag); \
	static TYPE_SECTION_ITERABLE(uintptr_t, varname, state_cache_##type##_ptr_val, varname)

#else

#define STATE_CACHE_BIND(tag, _type, varname) \
	const static uint8_t __keep __attribute__((section(".rodata.state_cache_" #_type "_tag"))) __attribute__((unused)) varname##_##tag = _STATE_CACHE_FULL_TAG_##_type(tag); \
	static _type __keep __attribute__((section(".bss.state_cache_" #_type "_val"))) varname
#endif

void state_cache_dump(void);
int state_cache_pack(uint8_t *buffer, int buffer_size);
int state_cache_dump_packed(uint8_t *buffer, int buffer_size);

#endif /* __CROS_EC_STATE_CACHE_H */