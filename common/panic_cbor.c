/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"
#include "gpio.h"
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

int panic_cbor_start_transaction(void)
{
	if (!panic_cbor_ptr)
		return EC_ERROR_UNAVAILABLE;

	if (!zcbor_new_backup(zcbor_state, 0)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

int panic_cbor_cancel_transaction(void)
{
	if (!panic_cbor_ptr)
		return EC_ERROR_UNAVAILABLE;

	if (!zcbor_process_backup(zcbor_state,
				  ZCBOR_FLAG_CONSUME | ZCBOR_FLAG_RESTORE,
				  0xFFFFFFFF)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

int panic_cbor_commit_transaction(void)
{
	if (!panic_cbor_ptr)
		return EC_ERROR_UNAVAILABLE;

	if (remaining_capacity() < (map_nest_level + list_nest_level)) {
		panic_cbor_cancel_transaction();
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
	return panic_cbor_map_start();
}

int panic_cbor_close(struct panic_data *pdata)
{
	int rv;
	if (!panic_cbor_ptr)
		return EC_ERROR_UNAVAILABLE;

	panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_CLOSED;
	rv = panic_cbor_map_end();
	panic_cbor_ptr->length = current_length();
	return rv;
}

int panic_cbor_uint32(uint32_t value)
{
	if (!panic_cbor_ptr)
		return EC_ERROR_UNAVAILABLE;

	if (!zcbor_uint32_put(zcbor_state, value)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

int panic_cbor_uint64(uint64_t value)
{
	if (!panic_cbor_ptr)
		return EC_ERROR_UNAVAILABLE;

	if (!zcbor_uint64_put(zcbor_state, value)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

int panic_cbor_bool(bool value)
{
	if (!panic_cbor_ptr)
		return EC_ERROR_UNAVAILABLE;

	if (!zcbor_bool_put(zcbor_state, value)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

int panic_cbor_null(void *value)
{
	if (!panic_cbor_ptr)
		return EC_ERROR_UNAVAILABLE;

	if (value != NULL) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_UNEXPECTED_INPUT;
		return EC_ERROR_INVAL;
	}

	if (!zcbor_nil_put(zcbor_state, value)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}
int panic_cbor_str(const char *value)
{
	if (!panic_cbor_ptr)
		return EC_ERROR_UNAVAILABLE;

	if (value == NULL) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_INVALID_INPUT;
		return EC_ERROR_INVAL;
	}
	if (!zcbor_tstr_put_term(zcbor_state, value,
				 PANIC_CBOR_VALUE_MAX_LEN)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

int panic_cbor_uint32_array(const uint32_t *array, size_t len)
{
	if (!panic_cbor_ptr)
		return EC_ERROR_UNAVAILABLE;

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

	return EC_SUCCESS;
}

int panic_cbor_map_start(void)
{
	if (!panic_cbor_ptr)
		return EC_ERROR_UNAVAILABLE;

	if (!zcbor_map_start_encode(zcbor_state, 0)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	map_nest_level += 1;
	return EC_SUCCESS;
}

int panic_cbor_map_end(void)
{
	if (!panic_cbor_ptr)
		return EC_ERROR_UNAVAILABLE;
	if (map_nest_level == 0) {
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
	if (!panic_cbor_ptr)
		return EC_ERROR_UNAVAILABLE;

	if (!zcbor_list_start_encode(zcbor_state, 0)) {
		panic_cbor_ptr->flags |= PANIC_CBOR_FLAG_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
	list_nest_level += 1;
	return EC_SUCCESS;
}

int panic_cbor_list_end(void)
{
	if (!panic_cbor_ptr)
		return EC_ERROR_UNAVAILABLE;

	if (map_nest_level <= 0) {
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

/* Some static values cannot be read from panic handler because of mallocs,
 * which must not be used from a handler context. These values are cached during
 * init here.
 */
char ro_version[32];
char rw_version[32];
static void panic_cbor_cache_init(void)
{
	memcpy(ro_version, system_get_version(EC_IMAGE_RO), sizeof(ro_version));
	memcpy(rw_version, system_get_version(EC_IMAGE_RW), sizeof(rw_version));
}
DECLARE_HOOK(HOOK_INIT, panic_cbor_cache_init, HOOK_PRIO_DEFAULT);

__maybe_unused static int fill_gpios_by_index(void)
{
	PANIC_CBOR_LABEL(PANIC_CBOR_LABEL_GPIO);
	panic_cbor_list_start();
	for (int i = 0; i < GPIO_COUNT; i++) {
		if (!gpio_is_implemented(i)) {
			panic_cbor_null(NULL);
			continue;
		}
		panic_cbor_bool(gpio_get_level(i));
	}
	panic_cbor_list_end();
	return EC_SUCCESS;
}

__maybe_unused static int fill_gpios_by_name(void)
{
	PANIC_CBOR_LABEL(PANIC_CBOR_LABEL_GPIO);
	panic_cbor_map_start();
	for (int i = 0; i < GPIO_COUNT; i++) {
		if (!gpio_is_implemented(i)) {
			continue;
		}
		PANIC_CBOR_LABEL_VALUE(gpio_get_name(i),
				       (bool)gpio_get_level(i));
	}
	panic_cbor_list_end();
	return EC_SUCCESS;
}

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
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_UPTIME, get_time().val);

	fill_gpios_by_index();

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
