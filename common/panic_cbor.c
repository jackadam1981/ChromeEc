/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_state.h"
#include "config.h"
#include "gpio.h"
#include "hooks.h"
#include "lid_switch.h"
#include "link_defs.h"
#include "panic.h"
#include "panic_cbor.h"
#include "power.h"
#include "system.h"
#include "task.h"
#include "zcbor_encode.h"

#ifdef CONFIG_PANIC_CBOR_DEBUG
#define PANIC_CBOR_CPRINTF(format, args...) panic_printf(format, ##args)
#else
#define PANIC_CBOR_CPRINTF(format, args...)
#endif

#define PANIC_CBOR_VERSION 1
#define PANIC_CBOR_LABEL_MAX_LEN 32
#define PANIC_CBOR_VALUE_MAX_LEN 256

/* Extra zcbor backups are needed for transactions */
#define EXTRA_ZCBOR_BACKUPS 2
static zcbor_state_t zcbor_state[2 + EXTRA_ZCBOR_BACKUPS];

static volatile panic_cbor_t _panic_cbor __aligned(4) __uncached
	__noinit_end_of_ram(_panic_cbor);

static volatile panic_cbor_t *panic_cbor_ptr = &_panic_cbor;

static uint16_t map_nest_level = 0;
static uint16_t list_nest_level = 0;
static bool panic_cbor_initialized = false;

static inline size_t current_length(void)
{
	return (size_t)zcbor_state->payload - (size_t)panic_cbor_ptr->data;
}

static inline size_t remaining_capacity(void)
{
	return (size_t)zcbor_state->payload_end - (size_t)zcbor_state->payload;
}

static uint32_t calc_checksum(void)
{
	uint32_t checksum = 0;
	for (int i = 0;
	     i < sizeof(panic_cbor_t) - sizeof(panic_cbor_ptr->checksum); i++) {
		checksum += ((uint8_t *)panic_cbor_ptr)[i];
	}
	return checksum;
}

static bool panic_cbor_is_valid(void)
{
	return ((panic_cbor_ptr->version = PANIC_CBOR_VERSION) &&
		(panic_cbor_ptr->length > 0) &&
		(panic_cbor_ptr->length <= panic_cbor_ptr->capacity) &&
		(panic_cbor_ptr->status & PANIC_CBOR_STATUS_OPENED) &&
		(panic_cbor_ptr->status & PANIC_CBOR_STATUS_CLOSED) &&
		(panic_cbor_ptr->checksum == calc_checksum()));
}

static int check_zcbor_error(void)
{
	switch (zcbor_pop_error(zcbor_state)) {
	case ZCBOR_SUCCESS:
		return EC_SUCCESS;
	case ZCBOR_ERR_NO_PAYLOAD:
		PANIC_CBOR_CPRINTF("Overflow!\n");
		panic_cbor_ptr->errors |= PANIC_CBOR_ERROR_OVERFLOW;
		return EC_ERROR_OVERFLOW;
	default:
		PANIC_CBOR_CPRINTF("Encode error: %d\n", err);
		panic_cbor_ptr->errors |= PANIC_CBOR_ERROR_ENCODE_ERROR;
		return EC_ERROR_UNKNOWN;
	}
}

int panic_cbor_open_append(void)
{
	if (!panic_cbor_is_valid()) {
		PANIC_CBOR_CPRINTF("Not valid, cannot reopen\n");
		return EC_ERROR_INVAL;
	}
	volatile uint8_t *data_ptr =
		(panic_cbor_ptr->data + (panic_cbor_ptr->length - 1));
	/* Last byte should be 0xff, aka map end.
	 * This byte will be overwritten to reopen map.
	 */
	if (*(data_ptr) != 0xff) {
		PANIC_CBOR_CPRINTF("Map end is missing\n");
		return EC_ERROR_INVAL;
	}
	panic_cbor_ptr->status &= ~PANIC_CBOR_STATUS_CLOSED;
	panic_cbor_ptr->status |= PANIC_CBOR_STATUS_APPENDED;
	zcbor_new_encode_state(
		zcbor_state, ARRAY_SIZE(zcbor_state), (uint8_t *)data_ptr,
		(panic_cbor_ptr->capacity - (panic_cbor_ptr->length - 1)), 0);
	panic_cbor_ptr->checksum = 0;
	map_nest_level = 1;
	list_nest_level = 0;
	return EC_SUCCESS;
}

int panic_cbor_open(void)
{
	panic_cbor_ptr->version = PANIC_CBOR_VERSION;
	panic_cbor_ptr->capacity = CONFIG_PANIC_CBOR_CAPACITY;
	panic_cbor_ptr->length = 0;
	panic_cbor_ptr->checksum = 0;
	panic_cbor_ptr->status = PANIC_CBOR_STATUS_OPENED;
	panic_cbor_ptr->errors = 0;
	memset((void *)panic_cbor_ptr->data, 0, CONFIG_PANIC_CBOR_CAPACITY);
	zcbor_new_encode_state(zcbor_state, ARRAY_SIZE(zcbor_state),
			       (uint8_t *)panic_cbor_ptr->data,
			       CONFIG_PANIC_CBOR_CAPACITY, 0);
	map_nest_level = 0;
	list_nest_level = 0;
	/* Start root map */
	return panic_cbor_map_start();
}

/* Some static values cannot be read from panic handler because of mutexes.
 * These values are cached during init here.
 */
char ro_version[32];
char rw_version[32];
static void panic_cbor_cache_init(void)
{
	memcpy(ro_version, system_get_version(EC_IMAGE_RO), sizeof(ro_version));
	memcpy(rw_version, system_get_version(EC_IMAGE_RW), sizeof(rw_version));
}

static int handle_watchddog_reset(void)
{
	if (panic_cbor_open_append() != EC_SUCCESS) {
		PANIC_CBOR_CPRINTF("Failed to open panic cbor for append\n");
		if (panic_cbor_open() != EC_SUCCESS) {
			PANIC_CBOR_CPRINTF("Failed to open panic cbor\n");
			return EC_ERROR_UNKNOWN;
		}
	}

	PANIC_CBOR_LABEL_MAP_START(PANIC_CBOR_LABEL_POST_WATCHDOG_RESET);
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_RESET_FLAGS,
			       system_get_reset_flags());
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_LID_OPEN,
			       (bool)(lid_is_open()));
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_UPTIME_US, get_time().val);
	PANIC_CBOR_MAP_END();
	panic_cbor_close();

	return EC_SUCCESS;
}

static void panic_cbor_init(void)
{
	/* Initialize may only run once */
	if (panic_cbor_initialized)
		return;
	panic_cbor_initialized = true;

	panic_cbor_cache_init();

	/* Handle watchdog reset */
	if (system_get_reset_flags() & EC_RESET_FLAG_WATCHDOG)
		handle_watchddog_reset();
}
DECLARE_HOOK(HOOK_INIT, panic_cbor_init, HOOK_PRIO_DEFAULT);

int panic_cbor_close(void)
{
	int rv;
	panic_cbor_ptr->status |= PANIC_CBOR_STATUS_CLOSED;
	rv = panic_cbor_map_end();
	panic_cbor_ptr->length = current_length();
	panic_cbor_ptr->checksum = calc_checksum();
	return rv;
}

int panic_cbor_start_transaction(void)
{
	zcbor_new_backup(zcbor_state, 0);
	return check_zcbor_error();
}

int panic_cbor_cancel_transaction(void)
{
	zcbor_process_backup(zcbor_state,
			     ZCBOR_FLAG_CONSUME | ZCBOR_FLAG_RESTORE,
			     0xFFFFFFFF);
	return check_zcbor_error();
}

int panic_cbor_commit_transaction(void)
{
	if (remaining_capacity() < (map_nest_level + list_nest_level)) {
		panic_cbor_cancel_transaction();
		return EC_ERROR_OVERFLOW;
	}
	zcbor_process_backup(zcbor_state,
			     ZCBOR_FLAG_CONSUME | ZCBOR_FLAG_KEEP_PAYLOAD,
			     0xFFFFFFFF);
	return check_zcbor_error();
}

int panic_cbor_int32(int32_t value)
{
	zcbor_int32_put(zcbor_state, value);
	return check_zcbor_error();
}

int panic_cbor_uint32(uint32_t value)
{
	zcbor_uint32_put(zcbor_state, value);
	return check_zcbor_error();
}

int panic_cbor_uint64(uint64_t value)
{
	zcbor_uint64_put(zcbor_state, value);
	return check_zcbor_error();
}

int panic_cbor_bool(bool value)
{
	zcbor_bool_put(zcbor_state, value);
	return check_zcbor_error();
}

int panic_cbor_null(void *value)
{
	zcbor_nil_put(zcbor_state, value);
	return check_zcbor_error();
}

int panic_cbor_str(const char *value)
{
	zcbor_tstr_put_term(zcbor_state, value, PANIC_CBOR_LABEL_MAX_LEN);
	return check_zcbor_error();
}

int panic_cbor_uint32_array(const uint32_t *array, size_t len)
{
	if (zcbor_list_start_encode(zcbor_state, len)) {
		zcbor_multi_encode(len, (zcbor_encoder_t *)zcbor_uint32_encode,
				   zcbor_state, array, 4);
		zcbor_list_end_encode(zcbor_state, len);
	}
	return check_zcbor_error();
}

int panic_cbor_map_start(void)
{
	if (zcbor_map_start_encode(zcbor_state, 0)) {
		map_nest_level += 1;
		return EC_SUCCESS;
	}
	return check_zcbor_error();
}

int panic_cbor_map_end(void)
{
	if (map_nest_level <= 0) {
		panic_cbor_ptr->errors |= PANIC_CBOR_ERROR_INVALID_INPUT;
		return EC_ERROR_INVAL;
	}
	zcbor_map_end_encode(zcbor_state, 0);
	map_nest_level -= 1;
	return check_zcbor_error();
}

int panic_cbor_list_start(void)
{
	zcbor_list_start_encode(zcbor_state, 0);
	list_nest_level += 1;
	return check_zcbor_error();
}

int panic_cbor_list_end(void)
{
	if (list_nest_level <= 0) {
		panic_cbor_ptr->errors |= PANIC_CBOR_ERROR_INVALID_INPUT;
		return EC_ERROR_INVAL;
	}
	zcbor_list_end_encode(zcbor_state, 0);
	list_nest_level -= 1;
	return check_zcbor_error();
}

__maybe_unused static int fill_gpios_by_index(void)
{
	PANIC_CBOR_LABEL(PANIC_CBOR_LABEL_GPIOS);
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
	PANIC_CBOR_LABEL(PANIC_CBOR_LABEL_GPIOS);
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

int panic_cbor_fill_charge_state(void)
{
	struct charge_state_data *charge_state = charge_get_status();
	if (!charge_state)
		return EC_ERROR_UNKNOWN;

	PANIC_CBOR_LABEL_MAP_START(PANIC_CBOR_LABEL_CHARGE_STATE);

	PANIC_CBOR_LABEL_VALUE("ts", charge_state->ts.val);
	PANIC_CBOR_LABEL_VALUE("ac", (bool)charge_state->ac);
	PANIC_CBOR_LABEL_VALUE("state", charge_state->state);
	PANIC_CBOR_LABEL_VALUE("req_mV", charge_state->requested_voltage);
	PANIC_CBOR_LABEL_VALUE("req_mA", charge_state->requested_current);
	PANIC_CBOR_LABEL_VALUE("des_input_mA",
			       charge_state->desired_input_current);
#ifdef CONFIG_CHARGER_OTG
	PANIC_CBOR_LABEL_VALUE("out_mA", charge_state->output_current);
#endif

	PANIC_CBOR_LABEL_MAP_START("chg");
	PANIC_CBOR_LABEL_VALUE("mV", charge_state->chg.current);
	PANIC_CBOR_LABEL_VALUE("mA", charge_state->chg.voltage);
	PANIC_CBOR_LABEL_VALUE("in_mA", charge_state->chg.input_current);
	PANIC_CBOR_LABEL_VALUE("status", charge_state->chg.status);
	PANIC_CBOR_LABEL_VALUE("option", charge_state->chg.option);
	PANIC_CBOR_LABEL_VALUE("flags", charge_state->chg.flags);
	PANIC_CBOR_MAP_END(); /* "chg" */

	PANIC_CBOR_LABEL_MAP_START("batt");
	PANIC_CBOR_LABEL_VALUE(
		"C", DECI_KELVIN_TO_CELSIUS(charge_state->batt.temperature));
	PANIC_CBOR_LABEL_VALUE("state", charge_state->batt.state_of_charge);
	PANIC_CBOR_LABEL_VALUE("mV", charge_state->batt.voltage);
	PANIC_CBOR_LABEL_VALUE("mA", charge_state->batt.current);
	PANIC_CBOR_LABEL_VALUE("des_mV", charge_state->batt.desired_voltage);
	PANIC_CBOR_LABEL_VALUE("des_mA", charge_state->batt.desired_current);
	PANIC_CBOR_LABEL_VALUE("flags", charge_state->batt.flags);
	PANIC_CBOR_LABEL_VALUE("mAh", charge_state->batt.remaining_capacity);
	PANIC_CBOR_LABEL_VALUE("full_mAh", charge_state->batt.full_capacity);
	PANIC_CBOR_LABEL_VALUE("display", charge_state->batt.display_charge);
	PANIC_CBOR_LABEL_VALUE("present", (bool)charge_state->batt.is_present);
	PANIC_CBOR_LABEL_VALUE("status", charge_state->batt.status);
	PANIC_CBOR_LABEL_VALUE("flags", charge_state->batt.flags);
	PANIC_CBOR_LABEL_VALUE("charging",
			       (bool)charge_state->batt_is_charging);
	PANIC_CBOR_MAP_END(); /* "batt" */

#ifdef CONFIG_OCPC
	PANIC_CBOR_LABEL_MAP_START("ocpc");
	PANIC_CBOR_LABEL_VALUE("active_chip",
			       charge_state->ocpc.active_chg_chip);
	PANIC_CBOR_LABEL_ARRAY("flags", charge_state->ocpc.chg_flags,
			       CONFIG_USB_PD_PORT_MAX_COUNT);
	PANIC_CBOR_MAP_END(); /* "ocpc" */
#endif

	PANIC_CBOR_MAP_END(); /*PANIC_CBOR_LABEL_CHARGE_STATE*/

	return EC_SUCCESS;
}

int panic_cbor_fill_common(void)
{
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_RESET_FLAGS,
			       system_get_reset_flags());
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_LID_OPEN,
			       (bool)(lid_is_open()));
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_ACTIVE_IMAGE,
			       system_get_image_copy_string());
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_RO_VERSION, ro_version);
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_RW_VERSION, rw_version);
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_UPTIME_US, get_time().val);
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_CURRENT_TASK,
			       task_get_current());

	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_POWER_STATE, power_get_state());
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_POWER_STATE_NAME,
			       power_get_state_name());
	PANIC_CBOR_LABEL_VALUE(PANIC_CBOR_LABEL_POWER_SIGNALS,
			       power_get_signals());

	if (IS_ENABLED(CONFIG_BATTERY))
		panic_cbor_fill_charge_state();

	fill_gpios_by_index();

	return EC_SUCCESS;
}

#ifdef CONFIG_PANIC_CBOR_DEBUG

int panic_cbor_dump(void)
{
	ccprintf("Version: %d\n", panic_cbor_ptr->version);
	ccprintf("Capacity: %d\n", panic_cbor_ptr->capacity);
	ccprintf("Length: %d\n", panic_cbor_ptr->length);
	ccprintf("Status: %04X\n", panic_cbor_ptr->status);
	ccprintf("Errors: %04X\n", panic_cbor_ptr->errors);
	ccprintf("Expected Checksum: %x\n", panic_cbor_ptr->checksum);
	ccprintf("Calculated Checksum: %x\n", calc_checksum());
	ccprintf("Valid: %d\n", panic_cbor_is_valid());
	ccprintf("Data: ");
	size_t dump_length = panic_cbor_ptr->length > 0 ?
				     panic_cbor_ptr->length :
				     panic_cbor_ptr->capacity;
	for (int i = 0; i < dump_length; i++) {
		if (i % 16 == 0) {
			ccprintf("\n");
			cflush();
		}
		ccprintf("%02x ", panic_cbor_ptr->data[i]);
	}
	ccprintf("\n");

	return EC_SUCCESS;
}

static int command_panic_cbor(int argc, const char **argv)
{
	panic_cbor_dump();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(paniccbor, command_panic_cbor, "", "Panic CBOR");

#endif
