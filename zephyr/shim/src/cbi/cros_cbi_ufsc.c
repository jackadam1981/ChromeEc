/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "cros_board_info.h"
#include "cros_cbi.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(cros_cbi_ufsc, LOG_LEVEL_ERR);

#define CBI_UFSC_COMPAT cros_ec_cbi_ufsc
#define CBI_UFSC_NODE DT_INST(0, CBI_UFSC_COMPAT)

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(CBI_UFSC_COMPAT) == 1,
	     "More than one CBI UFSC node defined");

#define DT_DRV_COMPAT cros_ec_cbi_ufsc_value

/* --- Compile-time DTS validation --- */

#define VALIDATE_UFSC_FIELD(id)                                               \
	BUILD_ASSERT(DT_PROP_LEN(id, start) == 1,                             \
		     "UFSC field has discontiguous bits, not yet supported"); \
	BUILD_ASSERT(DT_PROP_BY_IDX(id, start, 0) <                           \
			     (CBI_UFSC_DATA_COUNT * 32),                      \
		     "UFSC start bit is out of bounds (must be < 160)");      \
	BUILD_ASSERT(DT_PROP_BY_IDX(id, size, 0) <= 8,                        \
		     "UFSC field size cannot exceed 8 bits");                 \
	BUILD_ASSERT(DT_PROP_LEN(id, start) == DT_PROP_LEN(id, size),         \
		     "UFSC start and size arrays must have same length");     \
	BUILD_ASSERT(                                                         \
		(DT_PROP_BY_IDX(id, start, 0) / 32) ==                        \
			((DT_PROP_BY_IDX(id, start, 0) +                      \
			  DT_PROP_BY_IDX(id, size, 0) - 1) /                  \
			 32),                                                 \
		"UFSC field crosses a 32-bit boundary, which is not allowed.");

/* Apply validation to all child nodes of cbi-ufsc */
DT_FOREACH_CHILD_STATUS_OKAY(CBI_UFSC_NODE, VALIDATE_UFSC_FIELD)

#define VALIDATE_UFSC_VALUE(inst)                                              \
	BUILD_ASSERT(DT_INST_PROP(inst, value) <                               \
			     (1 << DT_PROP_BY_IDX(                             \
				      DT_PARENT(DT_DRV_INST(inst)), size, 0)), \
		     "UFSC value is too large for its parent field size");

/* Apply validation to all -value nodes */
DT_INST_FOREACH_STATUS_OKAY(VALIDATE_UFSC_VALUE)

/* --- Data Structures --- */

static struct cbi_ufsc cached_ufsc;
static bool cached_ufsc_ready;

#define CBI_UFSC_VALUE_ARRAY_ID(id) \
	[CBI_UFSC_VALUE_ID(id)] = DT_PROP(id, value),
#define CBI_UFSC_VALUE_ARRAY(inst) CBI_UFSC_VALUE_ARRAY_ID(DT_DRV_INST(inst))

static const uint8_t ufsc_values[] = { DT_INST_FOREACH_STATUS_OKAY(
	CBI_UFSC_VALUE_ARRAY) };

/* --- Device Tree Parsing Macros --- */

#define CBI_UFSC_PARENT_FIELD_CASE(inst)                                      \
	case CBI_UFSC_VALUE_ID(DT_DRV_INST(inst)):                            \
		start = (uint8_t)DT_PROP_BY_IDX(DT_PARENT(DT_DRV_INST(inst)), \
						start, 0);                    \
		size = (uint8_t)DT_PROP_BY_IDX(DT_PARENT(DT_DRV_INST(inst)),  \
					       size, 0);                      \
		break;

#define CBI_UFSC_INIT_APPLY_DEFAULT(inst)                                 \
	do {                                                              \
		if (DT_INST_PROP(inst, default)) {                        \
			const uint8_t start = (uint8_t)DT_PROP_BY_IDX(    \
				DT_PARENT(DT_DRV_INST(inst)), start, 0);  \
			const uint8_t size = (uint8_t)DT_PROP_BY_IDX(     \
				DT_PARENT(DT_DRV_INST(inst)), size, 0);   \
			const uint8_t val = DT_INST_PROP(inst, value);    \
			write_ufsc_field(&cached_ufsc, start, size, val); \
		}                                                         \
	} while (0);

/* --- Internal Helper Functions --- */

static inline uint8_t read_ufsc_field(const struct cbi_ufsc *ufsc,
				      uint8_t start, uint8_t size)
{
	uint8_t data_index = start / 32;
	uint8_t bit_offset = start % 32;
	uint32_t mask = BIT_MASK(size);

	return (ufsc->data[data_index] >> bit_offset) & mask;
}

static inline void write_ufsc_field(struct cbi_ufsc *ufsc, uint8_t start,
				    uint8_t size, uint8_t value)
{
	uint8_t data_index = start / 32;
	uint8_t bit_offset = start % 32;
	uint32_t mask = BIT_MASK(size);
	uint32_t temp_val;

	/* Perform a read-modify-write on the single uint32_t data */
	temp_val = ufsc->data[data_index];
	temp_val &= ~(mask << bit_offset);
	temp_val |= ((uint32_t)value & mask) << bit_offset;
	ufsc->data[data_index] = temp_val;
}

static int get_parent_field_value(struct cbi_ufsc ufsc,
				  enum cbi_ufsc_value_id value_id,
				  uint8_t *value)
{
	uint8_t start = 0;
	uint8_t size = 0;

	/*
	 * This switch statement is built at compile time and maps a value_id
	 * enum to its parent field's start and size properties.
	 */
	switch (value_id) {
		DT_INST_FOREACH_STATUS_OKAY(CBI_UFSC_PARENT_FIELD_CASE)
	default:
		return -EINVAL;
	}

	*value = read_ufsc_field(&ufsc, start, size);
	return 0;
}

/* --- Public API --- */

void cros_cbi_ufsc_init(void)
{
	if (cbi_get_ufsc(&cached_ufsc) != EC_SUCCESS) {
		cached_ufsc = (struct cbi_ufsc){ 0 };
		LOG_WRN("CBI: UFSC not found, using defaults.");

		/* Apply default values specified in DTS */
		DT_INST_FOREACH_STATUS_OKAY(CBI_UFSC_INIT_APPLY_DEFAULT)
	}
	cached_ufsc_ready = true;
	LOG_INF("Read CBI UFSC: 0x%08x 0x%08x 0x%08x 0x%08X 0x%08x",
		cached_ufsc.data[0], cached_ufsc.data[1], cached_ufsc.data[2],
		cached_ufsc.data[3], cached_ufsc.data[4]);
}

test_mockable bool cros_cbi_ufsc_check_match(enum cbi_ufsc_value_id value_id)
{
	uint8_t cbi_val;

	if (!cached_ufsc_ready) {
		LOG_ERR("CBI UFSC read before init");
		return false;
	}

	if (get_parent_field_value(cached_ufsc, value_id, &cbi_val) != 0) {
		return false;
	}

	return cbi_val == ufsc_values[value_id];
}
