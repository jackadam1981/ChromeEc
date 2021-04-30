/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cbi_ssfc.h"
#include <drivers/cros_cbi.h>
#include "cros_board_info.h"
#include "hooks.h"
#include "motionsense_sensors.h"
#include "motion_sense.h"
#include <logging/log.h>

LOG_MODULE_REGISTER(cbi_ssfc_shim);

/* CBI SSFC part */

#define DT_DRV_COMPAT named_cbi_ssfc_value

#define CBI_SSFC_NODE			DT_PATH(cbi_ssfc)

#define SSFC_INIT_DEFAULT_ID(id)                                               \
	do {                                                                   \
		if (DT_PROP(id, default)) {                                    \
			cached_ssfc.CBI_SSFC_UNION_ENTRY_NAME(DT_PARENT(id)) = \
			    DT_PROP(id, value);                                \
		}                                                              \
	} while (0);


#define SSFC_INIT_DEFAULT(inst) \
	SSFC_INIT_DEFAULT_ID(DT_DRV_INST(inst))

#define CBI_SSFC_VALUE_ARRAY_ID(id) \
	[CBI_SSFC_VALUE_ID(id)] = DT_PROP(id, value),

#define CBI_SSFC_VALUE_ARRAY(inst) \
	CBI_SSFC_VALUE_ARRAY_ID(DT_DRV_INST(inst))


#define CBI_SSFC_PARENT_VALUE_CASE_GENERATE(value_id, value_parent) \
	case value_id: \
		return value_parent;

#define CBI_SSFC_PARENT_VALUE_CASE_ID(id) \
	CBI_SSFC_PARENT_VALUE_CASE_GENERATE(CBI_SSFC_VALUE_ID(id), \
		cached_ssfc.CBI_SSFC_UNION_ENTRY_NAME(DT_PARENT(id)))

#define CBI_SSFC_PARENT_VALUE_CASE(inst) \
	CBI_SSFC_PARENT_VALUE_CASE_ID(DT_DRV_INST(inst))


#define CBI_SSFC_UNION_ENTRY_NAME(id)	DT_CAT(cbi_ssfc_, id)
#define CBI_SSFC_UNION_ENTRY(id)               \
	uint32_t CBI_SSFC_UNION_ENTRY_NAME(id) \
		: DT_PROP(id, size);

#define CBI_SSFC_PLUS_FIELD_SIZE(id)	+ DT_PROP(id, size)
#define CBI_SSFC_FIELDS_SIZE                                         \
	(0 COND_CODE_1(DT_NODE_EXISTS(CBI_SSFC_NODE),                \
		       (DT_FOREACH_CHILD(CBI_SSFC_NODE,              \
					 CBI_SSFC_PLUS_FIELD_SIZE)), \
		       ()))

BUILD_ASSERT(CBI_SSFC_FIELDS_SIZE <= 32, "CBI SSFS is bigger than 32 bits");

/*
 * Define union bit fields based on the device tree entries. Example:
 * cbi-ssfc {
 *	compatible = "named-cbi-ssfc";
 *
 *	base_sensor {
 *		enum-name = "BASE_SENSOR";
 *		size = <3>;
 *		bmi160 {
 *			compatible = "named-cbi-ssfc-value";
 *			status = "okay";
 *
 *			value = <1>;
 *			devices = <>;
 *		};
 *	};
 *	lid_sensor {
 *		enum-name = "LID_SENSOR";
 *		size = <3>;
 *		bma255 {
 *			compatible = "named-cbi-ssfc-value";
 *			status = "okay";
 *
 *			value = <1>;
 *			devices = <&lid_accel>;
 *		};
 *	};
 *	lightbar {
 *		enum-name = "LIGHTBAR";
 *		size = <2>;
 *		10_led {
 *			compatible = "named-cbi-ssfc-value";
 *			status = "okay";
 *
 *			value = <1>;
 *			devices = <>;
 *		};
 *	};
 * };
 * Should be converted into
 * union cbi_ssfc {
 *	struct {
 *		uint32_t cbi_ssfc_DT_N_S_cbi_ssfc_S_base_sensor:3
 *		uint32_t cbi_ssfc_DT_N_S_cbi_ssfc_S_lid_sensor:3
 *		uint32_t cbi_ssfc_DT_N_S_cbi_ssfc_S_lightbar:2
 *		uint32_t reserved : 24;
 *	};
 *	uint32_t raw_value;
 * };
 */
union cbi_ssfc {
	struct {
#if DT_NODE_EXISTS(CBI_SSFC_NODE)
		DT_FOREACH_CHILD(CBI_SSFC_NODE, CBI_SSFC_UNION_ENTRY)
		uint32_t reserved : (32 - CBI_SSFC_FIELDS_SIZE);
#endif
	};
	uint32_t raw_value;
};

BUILD_ASSERT(sizeof(union cbi_ssfc) == sizeof(uint32_t),
	"CBI SSFS structure exceedes 32 bits");

// TODO: Add build assert to ensure no ssfc_value is not bigger than uint8_t_max
static const uint8_t ssfc_values[] = {
	DT_INST_FOREACH_STATUS_OKAY(CBI_SSFC_VALUE_ARRAY)
};
static union cbi_ssfc cached_ssfc;

static uint32_t cbi_ssfc_get_parent_field_value(enum cbi_ssfc_value_id value_id);
static int check_ssfc_match(const struct device *dev, enum cbi_ssfc_value_id value_id);

static void cbi_ssfc_init(void)
{
	if (cbi_get_ssfc(&cached_ssfc.raw_value) != EC_SUCCESS) {
		/* Default to values specified in DTS */
		DT_INST_FOREACH_STATUS_OKAY(SSFC_INIT_DEFAULT)
	}

	LOG_INF("Read CBI SSFC : 0x%08X \n", cached_ssfc.raw_value);

}
// TODO: Can be changed for init instead?
DECLARE_HOOK(HOOK_INIT, cbi_ssfc_init, HOOK_PRIO_FIRST);

static uint32_t cbi_ssfc_get_parent_field_value(enum cbi_ssfc_value_id value_id)
{
	switch(value_id) {
	DT_INST_FOREACH_STATUS_OKAY(CBI_SSFC_PARENT_VALUE_CASE)
	default:
		LOG_DBG("CBI SSFC parent field value not found: %d\n", value_id);
		return 0;
	}
}

static int check_ssfc_match(const struct device *dev, enum cbi_ssfc_value_id value_id)
{
	return cbi_ssfc_get_parent_field_value(value_id)==ssfc_values[value_id];
}

#undef DT_DRV_COMPAT

static int cbi_init(const struct device *dev)
{

// TODO: Uncomment if possible cbi_ssfc_init();

	return 0;
}

/* cros ec cbi driver registration */
static const struct cros_cbi_driver_api cros_cbi_driver_api = {
	.check_ssfc_match = check_ssfc_match,
};

uint8_t data_tmp;
uint8_t config_tmp;

DEVICE_DEFINE(cros_cbi, "cros_cbi", cbi_init, NULL,
	      &data_tmp, &config_tmp, PRE_KERNEL_1,
	      CONFIG_PLATFORM_EC_GPIO_INIT_PRIORITY, &cros_cbi_driver_api);
