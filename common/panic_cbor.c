/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"
#include "hooks.h"
#include "lid_switch.h"
#include "panic.h"
#include "panic_cbor.h"
#include "system.h"
#include "task.h"
#include "zcbor_encode.h"

#define PANIC_CBOR_VERSION 1
#define PANIC_CBOR_LABEL_MAX_LEN 32
#define PANIC_CBOR_VALUE_MAX_LEN 256

#define EXTRA_ZCBOR_BACKUPS 1
static zcbor_state_t zcbor_state[2 + EXTRA_ZCBOR_BACKUPS];

typedef enum {
	PANIC_CBOR_STATE_UNINITIALIZED = 0,
	PANIC_CBOR_STATE_OPEN_EXPECTING_LABEL,
	PANIC_CBOR_STATE_OPEN_EXPECTING_VALUE,
	PANIC_CBOR_STATE_CLOSED,
} panic_cbor_state_t;

static panic_cbor_state_t panic_cbor_state = PANIC_CBOR_STATE_UNINITIALIZED;
static struct panic_cbor *panic_cbor_ptr = NULL;
static uint16_t map_nest_level = 0;
static uint16_t list_nest_level = 0;

static inline size_t current_length(void)
{
	return (size_t)zcbor_state->payload - (size_t)panic_cbor_ptr->data;
}

static inline size_t remaining_capacity(void)
{
	return (size_t)zcbor_state->payload_end - (size_t)zcbor_state->payload;
}

static inline int start_transaction(void)
{
	if (panic_cbor_state == PANIC_CBOR_STATE_UNINITIALIZED)
		return EC_ERROR_UNAVAILABLE;

	if (!zcbor_new_backup(zcbor_state, 0)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

static inline int cancel_transaction(void)
{
	if (panic_cbor_state == PANIC_CBOR_STATE_UNINITIALIZED)
		return EC_ERROR_UNAVAILABLE;

	if (!zcbor_process_backup(zcbor_state,
				  ZCBOR_FLAG_CONSUME | ZCBOR_FLAG_RESTORE,
				  0xFFFFFFFF)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

static inline int end_transaction(void)
{
	if (panic_cbor_state == PANIC_CBOR_STATE_UNINITIALIZED)
		return EC_ERROR_UNAVAILABLE;

	if (remaining_capacity() < (map_nest_level + list_nest_level + 1)) {
		cancel_transaction();
		return EC_ERROR_OVERFLOW;
	}
	if (!zcbor_process_backup(zcbor_state,
				  ZCBOR_FLAG_CONSUME | ZCBOR_FLAG_KEEP_PAYLOAD,
				  0xFFFFFFFF)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

int panic_cbor_transaction(int (*funcs[])(const void *), const void *args[],
			   size_t len)
{
	int rv;
	rv = start_transaction();
	if (rv != EC_SUCCESS) {
		return rv;
	}

	for (int i = 0; i < len; i++) {
		rv = funcs[i](args[i]);
		if (rv != EC_SUCCESS) {
			cancel_transaction();
			return rv;
		}
	}
	rv = end_transaction();
	return rv;
}

int panic_cbor_open(struct panic_data *pdata)
{
	pdata->arch = PANIC_ARCH_CBOR;
	panic_cbor_ptr = &pdata->cbor;
	panic_cbor_ptr->version = PANIC_CBOR_VERSION;
	panic_cbor_ptr->capacity = CONFIG_PANIC_CBOR_CAPACITY;
	panic_cbor_ptr->length = 0;
	panic_cbor_ptr->flags = PANIC_CBOR_FLAG_OPENED;
	map_nest_level = 0;
	list_nest_level = 0;
	memset(pdata->cbor.data, 0, CONFIG_PANIC_CBOR_CAPACITY);
	zcbor_new_encode_state(zcbor_state, ARRAY_SIZE(zcbor_state),
			       panic_cbor_ptr->data, CONFIG_PANIC_CBOR_CAPACITY,
			       0);
	/* Start root map */
	if (!zcbor_map_start_encode(zcbor_state, 0)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	panic_cbor_state = PANIC_CBOR_STATE_OPEN_EXPECTING_LABEL;
	return EC_SUCCESS;
}

int panic_cbor_close(struct panic_data *pdata)
{
	if (panic_cbor_state == PANIC_CBOR_STATE_UNINITIALIZED)
		return EC_ERROR_UNAVAILABLE;
	if (panic_cbor_state == PANIC_CBOR_STATE_CLOSED) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_UNEXPECTED_INPUT;
		return EC_ERROR_INVAL;
	}
	panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_CLOSED;
	panic_cbor_state = PANIC_CBOR_STATE_CLOSED;
	/* End root map */
	if (!zcbor_map_end_encode(zcbor_state, 0)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	panic_cbor_ptr->length = current_length();
	return EC_SUCCESS;
}

int panic_cbor_str_label(const char *label)
{
	if (panic_cbor_state == PANIC_CBOR_STATE_UNINITIALIZED)
		return EC_ERROR_UNAVAILABLE;

	if (panic_cbor_state != PANIC_CBOR_STATE_OPEN_EXPECTING_LABEL) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_UNEXPECTED_INPUT;
		return EC_ERROR_INVAL;
	}

	if (label == NULL) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_INVALID_INPUT;
		return EC_ERROR_INVAL;
	}

	if (!zcbor_tstr_put_term(zcbor_state, label,
				 PANIC_CBOR_LABEL_MAX_LEN)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}

	panic_cbor_state = PANIC_CBOR_STATE_OPEN_EXPECTING_VALUE;

	return EC_SUCCESS;
}

int panic_cbor_enum_label(enum panic_cbor_label label)
{
	if (panic_cbor_state == PANIC_CBOR_STATE_UNINITIALIZED)
		return EC_ERROR_UNAVAILABLE;

	if (panic_cbor_state != PANIC_CBOR_STATE_OPEN_EXPECTING_LABEL) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_UNEXPECTED_INPUT;
		return EC_ERROR_INVAL;
	}

	if (label >= PANIC_CBOR_LABEL_COUNT) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_INVALID_INPUT;
		return EC_ERROR_INVAL;
	}

	if (!zcbor_uint32_put(zcbor_state, label)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}

	panic_cbor_state = PANIC_CBOR_STATE_OPEN_EXPECTING_VALUE;

	return EC_SUCCESS;
}

int panic_cbor_uint32(uint32_t value)
{
	if (panic_cbor_state == PANIC_CBOR_STATE_UNINITIALIZED)
		return EC_ERROR_UNAVAILABLE;

	if (panic_cbor_state != PANIC_CBOR_STATE_OPEN_EXPECTING_VALUE) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_UNEXPECTED_INPUT;
		return EC_ERROR_INVAL;
	}

	if (!zcbor_uint32_put(zcbor_state, value)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}

	panic_cbor_state = PANIC_CBOR_STATE_OPEN_EXPECTING_LABEL;

	return EC_SUCCESS;
}

int panic_cbor_enum_label_uint32(enum panic_cbor_label label, uint32_t value)
{
	int rv;

	rv = start_transaction();
	if (rv != EC_SUCCESS)
		return rv;

	rv = panic_cbor_enum_label(label);
	if (rv != EC_SUCCESS) {
		cancel_transaction();
		return rv;
	}

	rv = panic_cbor_uint32(value);
	if (rv != EC_SUCCESS) {
		cancel_transaction();
		return rv;
	}

	return end_transaction();
}

int panic_cbor_enum_label_uint64(enum panic_cbor_label label, uint64_t value)
{
	int rv;

	rv = start_transaction();
	if (rv != EC_SUCCESS)
		return rv;

	rv = panic_cbor_enum_label(label);
	if (rv != EC_SUCCESS) {
		cancel_transaction();
		return rv;
	}

	rv = panic_cbor_uint64(value);
	if (rv != EC_SUCCESS) {
		cancel_transaction();
		return rv;
	}

	return end_transaction();
}

int panic_cbor_enum_label_bool(enum panic_cbor_label label, uint32_t value)
{
	int rv;

	rv = start_transaction();
	if (rv != EC_SUCCESS)
		return rv;

	rv = panic_cbor_enum_label(label);
	if (rv != EC_SUCCESS) {
		cancel_transaction();
		return rv;
	}

	rv = panic_cbor_bool(value);
	if (rv != EC_SUCCESS) {
		cancel_transaction();
		return rv;
	}

	return end_transaction();
}

int panic_cbor_enum_label_str(enum panic_cbor_label label, const char *value)
{
	int rv;

	rv = start_transaction();
	if (rv != EC_SUCCESS)
		return rv;

	rv = panic_cbor_enum_label(label);
	if (rv != EC_SUCCESS) {
		cancel_transaction();
		return rv;
	}

	rv = panic_cbor_str(value);
	if (rv != EC_SUCCESS) {
		cancel_transaction();
		return rv;
	}

	return end_transaction();
}

int panic_cbor_uint64(uint64_t value)
{
	if (panic_cbor_state == PANIC_CBOR_STATE_UNINITIALIZED)
		return EC_ERROR_UNAVAILABLE;
	if (panic_cbor_state != PANIC_CBOR_STATE_OPEN_EXPECTING_VALUE) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_UNEXPECTED_INPUT;
		return EC_ERROR_INVAL;
	}
	if (!zcbor_uint64_put(zcbor_state, value)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	panic_cbor_state = PANIC_CBOR_STATE_OPEN_EXPECTING_LABEL;
	return EC_SUCCESS;
}

int panic_cbor_bool(bool value)
{
	if (panic_cbor_state == PANIC_CBOR_STATE_UNINITIALIZED)
		return EC_ERROR_UNAVAILABLE;
	if (panic_cbor_state != PANIC_CBOR_STATE_OPEN_EXPECTING_VALUE) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_UNEXPECTED_INPUT;
		return EC_ERROR_INVAL;
	}
	if (!zcbor_bool_put(zcbor_state, value)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	panic_cbor_state = PANIC_CBOR_STATE_OPEN_EXPECTING_LABEL;
	return EC_SUCCESS;
}

int panic_cbor_str(const char *value)
{
	if (panic_cbor_state == PANIC_CBOR_STATE_UNINITIALIZED)
		return EC_ERROR_UNAVAILABLE;
	if (panic_cbor_state != PANIC_CBOR_STATE_OPEN_EXPECTING_VALUE) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_UNEXPECTED_INPUT;
		return EC_ERROR_INVAL;
	}
	if (value == NULL) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_INVALID_INPUT;
		return EC_ERROR_INVAL;
	}
	if (!zcbor_tstr_put_term(zcbor_state, value,
				 PANIC_CBOR_VALUE_MAX_LEN)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	panic_cbor_state = PANIC_CBOR_STATE_OPEN_EXPECTING_LABEL;
	return EC_SUCCESS;
}

int panic_cbor_uint32_array(const uint32_t *array, size_t len)
{
	if (panic_cbor_state == PANIC_CBOR_STATE_UNINITIALIZED)
		return EC_ERROR_UNAVAILABLE;
	if (panic_cbor_state != PANIC_CBOR_STATE_OPEN_EXPECTING_VALUE) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_UNEXPECTED_INPUT;
		return EC_ERROR_INVAL;
	}
	if (!zcbor_list_start_encode(zcbor_state, len)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	if (!zcbor_multi_encode(len, (zcbor_encoder_t *)zcbor_uint32_encode,
				zcbor_state, array, 4)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	if (!zcbor_list_end_encode(zcbor_state, len)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	panic_cbor_state = PANIC_CBOR_STATE_OPEN_EXPECTING_LABEL;
	return EC_SUCCESS;
}

int panic_cbor_map_start(void)
{
	if (panic_cbor_state == PANIC_CBOR_STATE_UNINITIALIZED)
		return EC_ERROR_UNAVAILABLE;
	if (panic_cbor_state != PANIC_CBOR_STATE_OPEN_EXPECTING_VALUE) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_UNEXPECTED_INPUT;
		return EC_ERROR_INVAL;
	}
	if (!zcbor_map_start_encode(zcbor_state, 0)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	map_nest_level += 1;
	panic_cbor_state = PANIC_CBOR_STATE_OPEN_EXPECTING_LABEL;
	return EC_SUCCESS;
}

int panic_cbor_map_end(void)
{
	if (panic_cbor_state == PANIC_CBOR_STATE_UNINITIALIZED)
		return EC_ERROR_UNAVAILABLE;
	if (panic_cbor_state != PANIC_CBOR_STATE_OPEN_EXPECTING_LABEL ||
	    map_nest_level == 0) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_UNEXPECTED_INPUT;
		return EC_ERROR_INVAL;
	}
	if (!zcbor_map_end_encode(zcbor_state, 0)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	map_nest_level -= 1;
	return EC_SUCCESS;
}

int panic_cbor_list_start(void)
{
	if (panic_cbor_state == PANIC_CBOR_STATE_UNINITIALIZED)
		return EC_ERROR_UNAVAILABLE;
	if (panic_cbor_state != PANIC_CBOR_STATE_OPEN_EXPECTING_VALUE) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_UNEXPECTED_INPUT;
		return EC_ERROR_INVAL;
	}
	if (!zcbor_list_start_encode(zcbor_state, 0)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	list_nest_level += 1;
	panic_cbor_state = PANIC_CBOR_STATE_OPEN_EXPECTING_VALUE;
	return EC_SUCCESS;
}

int panic_cbor_list_end(void)
{
	if (panic_cbor_state == PANIC_CBOR_STATE_UNINITIALIZED)
		return EC_ERROR_UNAVAILABLE;
	if (panic_cbor_state != PANIC_CBOR_STATE_OPEN_EXPECTING_VALUE ||
	    list_nest_level == 0) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_UNEXPECTED_INPUT;
		return EC_ERROR_INVAL;
	}
	if (!zcbor_list_end_encode(zcbor_state, 0)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	list_nest_level -= 1;
	return EC_SUCCESS;
}

int _panic_cbor_str_label(const void *label)
{
	return panic_cbor_str_label((const char *)label);
}

int _panic_cbor_enum_label(const void *label)
{
	return panic_cbor_enum_label((enum panic_cbor_label)label);
}

int _panic_cbor_uint32(const void *value)
{
	return panic_cbor_uint32((uint32_t)value);
}

int _panic_cbor_str(const void *value)
{
	return panic_cbor_str((const char *)value);
}

int _panic_cbor_bool(const void *value)
{
	return panic_cbor_bool((bool)value);
}

/* Some static values cannot be read from panic handler because mallocs cannot
 * not be used from an interrupt handler context. These values are cached at
 * init.
 */
char ro_version[32];
char rw_version[32];
static void panic_cbor_cache_init(void)
{
	memcpy(ro_version, system_get_version(EC_IMAGE_RO), sizeof(ro_version));
	memcpy(rw_version, system_get_version(EC_IMAGE_RW), sizeof(rw_version));
}
DECLARE_HOOK(HOOK_INIT, panic_cbor_cache_init, HOOK_PRIO_DEFAULT);

int panic_cbor_fill_common(void)
{
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_RESET_FLAGS,
			       system_get_reset_flags());
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_LID_OPEN,
			       (bool)(lid_is_open()));
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_CURRENT_TASK,
			       task_get_current());
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_ACTIVE_IMAGE,
			       system_get_image_copy_string());
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_RO_VERSION, ro_version);
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_RW_VERSION, rw_version);

	return EC_SUCCESS;
}

#ifdef CONFIG_PANIC_CBOR_DEBUG

int panic_cbor_dump(struct panic_data *pdata)
{
	panic_printf("Version: %d\n", pdata->cbor.version);
	panic_printf("Capacity: %d\n", pdata->cbor.capacity);
	panic_printf("Length: %d\n", pdata->cbor.length);
	panic_printf("Flags: %04X\n", pdata->cbor.flags);
	panic_printf("Data: ");
	size_t dump_length = pdata->cbor.length > 0 ? pdata->cbor.length :
						      pdata->cbor.capacity;
	for (int i = 0; i < dump_length; i++) {
		panic_printf("%02x ", pdata->cbor.data[i]);
	}
	panic_printf("\n");

	return EC_SUCCESS;
}

#endif
