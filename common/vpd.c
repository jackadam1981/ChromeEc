/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "system.h"
#include "util.h"
#include "vpd.h"

#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

#define SYSJUMP_TAG_VPD		0xec

#ifdef SECTION_IS_RO
const struct vpd ro_vpd __keep __attribute__((section(".google"))) = {
	.data = {0},
};
#endif
const uint8_t *vpd_copy;

struct record {
	enum vpd_type type;
	uint8_t key_len;
	const uint8_t *key;
	uint8_t value_len;
	const uint8_t *value;
	const uint8_t *next;
};

/* Locals */

#ifdef SECTION_IS_RO
static void store_vpd(void)
{
	if (system_add_jump_tag(SYSJUMP_TAG_VPD, 0, sizeof(ro_vpd), &ro_vpd))
		CPRINTS("Failed to store VPD");
}
DECLARE_HOOK(HOOK_SYSJUMP, store_vpd, HOOK_PRIO_DEFAULT);
#endif

static int is_valid_record(const uint8_t *p)
{
	/* Only string type is supported */
	return *p == VPD_TYPE_STRING;
}

static int get_length(const uint8_t *p)
{
	int len = *(p + 1);
	/* Currently, we don't support 'more' bit in EC VPD. */
	return len < 128 ? len : -1;
}

static int is_pointer_valid(const uint8_t *p)
{
	return vpd_copy <= p && p < vpd_copy + CONFIG_RO_VPD_SIZE;
}

static int parse_record(const uint8_t *p, struct record *rec)
{
	int len;

	/* Type */
	if (!is_pointer_valid(p))
		return EC_ERROR_INVAL;
	if (!is_valid_record(p))
		return EC_ERROR_INVAL;
	rec->type = *p++;

	/* Key */
	if (!is_pointer_valid(p))
		return EC_ERROR_INVAL;
	len = get_length(p++);
	if (len < 0)
		return EC_ERROR_INVAL;
	rec->key_len = len;
	if (!is_pointer_valid(p + len))
		return EC_ERROR_INVAL;
	rec->key = p;
	p += len;

	/* Value */
	if (!is_pointer_valid(p))
		return EC_ERROR_INVAL;
	len = get_length(p++);
	if (len < 0)
		return EC_ERROR_INVAL;
	rec->value_len = len;
	if (!is_pointer_valid(p + len))
		return EC_ERROR_INVAL;
	rec->value = p;
	p += len;

	/* Next record */
	rec->next = p;

	return EC_SUCCESS;
}

static int find_by_key(enum vpd_type type, const uint8_t *key,
		       struct record *rec)
{
	const uint8_t *p = vpd_copy;

	while (is_pointer_valid(p)) {
		if (parse_record(p, rec))
			return EC_ERROR_INVAL;
		if (!strncmp(rec->key, key, rec->key_len))
			return EC_SUCCESS;
		p = rec->next;
	}
	/* Not found */
	return EC_ERROR_INVAL;
}

/* APIs */

void vpd_init(void)
{
#ifdef SECTION_IS_RO
	vpd_copy = (const uint8_t *)&ro_vpd;
#else
	int size;
	vpd_copy = (const void*)system_get_jump_tag(SYSJUMP_TAG_VPD, 0, &size);
	if (!vpd_copy || size != sizeof(*vpd_copy)) {
		CPRINTS("Failed to load VPD");
		return;
	}
#endif
}

int vpd_get(enum vpd_type type, const uint8_t *key,
	    uint8_t *value, uint8_t *value_len)
{
	struct record rec;

	if (find_by_key(type, key, &rec)) {
		CPRINTS("Failed to search key");
		return EC_ERROR_INVAL;
	}

	memcpy(value, rec.value, rec.value_len);
	*value_len = rec.value_len;
	return EC_SUCCESS;
}
