#include "system.h"

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <gpio_signal.h>

LOG_MODULE_DECLARE(board_id, LOG_LEVEL_INF);

#define DT_DRV_COMPAT intel_variant_gpio

struct intel_variant_gpio_data {
	const struct gpio_dt_spec *spec;
};

struct intel_variant_gpio_config {
	int specs_count;
	const struct gpio_dt_spec **specs;
};

struct intel_board_id_gpios {
	int board_version;
	const struct gpio_dt_spec **specs;
	int specs_count;
};

/* Locally define GPIO's that are part of a varant GPIO list for easier lookup.
 */
#define VAR_GPIO_DT_SPEC_GET_BY_IDX(node_id, prop, idx)           \
	{                                                         \
		.port = DEVICE_DT_GET(                            \
			DT_GPIO_CTLR_BY_IDX(node_id, prop, idx)), \
		.pin = DT_GPIO_PIN_BY_IDX(node_id, prop, idx),    \
	}

#define VARIANT_GPIO_SPEC_DEFINE(node_id, prop, idx)                        \
	const struct gpio_dt_spec DT_CAT5(node_id, _P_, prop, _IDX_, idx) = \
		VAR_GPIO_DT_SPEC_GET_BY_IDX(                                \
			DT_CAT5(node_id, _P_, prop, _IDX_, idx), gpios, 0);

#define VARIANT_GPIOS_SPEC_DEFINE(inst) \
	DT_INST_FOREACH_PROP_ELEM(inst, gpios, VARIANT_GPIO_SPEC_DEFINE)

DT_INST_FOREACH_STATUS_OKAY(VARIANT_GPIOS_SPEC_DEFINE)

/* Associate locally defined GPIO's reference with variant GPIO's. */
#define VARIANT_GPIO_SPEC_ARRAY_DEFINE(node_id, prop, idx) \
	&DT_CAT5(node_id, _P_, prop, _IDX_, idx),

#define VARIANT_GPIO_SPEC_ARRAYS_DEFINE(inst)                             \
	const struct gpio_dt_spec *DT_DRV_INST(inst)[] = {                \
		DT_INST_FOREACH_PROP_ELEM(inst, gpios,                    \
					  VARIANT_GPIO_SPEC_ARRAY_DEFINE) \
	};

DT_INST_FOREACH_STATUS_OKAY(VARIANT_GPIO_SPEC_ARRAYS_DEFINE)

/* Associate locally defined GPIO's reference with board ID GPIO's. */
#define BOARD_ID_VARIANT_GPIO_SPECS_ARRAY_ENTRY_DEFINE_(node_id, prop, idx) \
	&DT_CAT5(node_id, _P_, prop, _IDX_, idx),

#define BOARD_ID_VARIANT_GPIO_SPECS_ARRAY_ENTRY_DEFINE(node_id) &node_id,

#define BOARD_ID_VARIANT_GPIO_SPECS_ARRAY_DEFINE(node_id, prop, idx) \
	BOARD_ID_VARIANT_GPIO_SPECS_ARRAY_ENTRY_DEFINE(              \
		DT_CAT5(node_id, _P_, prop, _IDX_, idx))

#define BOARD_ID_VARIANT_GPIO_SPECS_DEFINE(node_id)                            \
	const struct gpio_dt_spec *board_id_##node_id##_variant_gpios[] = {    \
		DT_FOREACH_PROP_ELEM(node_id, variant_gpios,                   \
				     BOARD_ID_VARIANT_GPIO_SPECS_ARRAY_DEFINE) \
	};

DT_FOREACH_STATUS_OKAY(intel_board_id, BOARD_ID_VARIANT_GPIO_SPECS_DEFINE)

#define BOARD_ID_VARIANT_GPIO_CONFIG_DEFINE(node_id)                \
	{                                                           \
		.board_version = DT_PROP(node_id, board_version),   \
		.specs_count = DT_PROP_LEN(node_id, variant_gpios), \
		.specs = board_id_##node_id##_variant_gpios,        \
	},

static const struct intel_board_id_gpios board_ids_gpios[] = {
	DT_FOREACH_STATUS_OKAY(intel_board_id,
			       BOARD_ID_VARIANT_GPIO_CONFIG_DEFINE)
};

const struct gpio_dt_spec *intel_variant_gpio_get_dt(const struct device *dev)
{
	struct intel_variant_gpio_data *data = dev->data;

	return data->spec;
}

static int intel_variant_gpio_init(const struct device *dev)
{
	struct intel_variant_gpio_data *data = dev->data;
	const struct intel_variant_gpio_config *config = dev->config;
	static const struct intel_board_id_gpios *board_id = NULL;
	static bool board_id_searched = false;

	/* Search only once for board_id, and report if not found. */
	if (board_id_searched == false) {
		for (int i = 0; i < ARRAY_SIZE(board_ids_gpios); i++) {
			const struct intel_board_id_gpios *cur_board_id =
				&board_ids_gpios[i];

			if (system_get_board_version() ==
			    cur_board_id->board_version) {
				board_id = cur_board_id;
				LOG_DBG("Board ID found: 0x%02X",
					board_id->board_version);
				break;
			}
		}
		board_id_searched = true;
		if (board_id == NULL) {
			LOG_WRN("Board ID in CBI not found.");
		}
	}

	for (int i = 0; board_id && i < config->specs_count; i++) {
		for (int j = 0; j < board_id->specs_count; j++) {
			if (config->specs[i] == board_id->specs[j]) {
				data->spec = board_id->specs[j];
				return 0;
			}
		}
	}

	if (data->spec == NULL) {
		/* Give first option as default */
		data->spec = config->specs[0];
	}

	return 0;
}

#define VARIANT_GPIO_DEV_DEFINE(inst)                                   \
	static struct intel_variant_gpio_data variant_gpio_data_##inst; \
	static const struct intel_variant_gpio_config                   \
		variant_gpio_config_##inst = {                          \
			.specs_count = DT_INST_PROP_LEN(inst, gpios),   \
			.specs = DT_DRV_INST(inst),                     \
		};                                                      \
	DEVICE_DT_INST_DEFINE(inst, intel_variant_gpio_init, NULL,      \
			      &variant_gpio_data_##inst,                \
			      &variant_gpio_config_##inst, POST_KERNEL, \
			      CONFIG_PLATFORM_EC_GPIO_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(VARIANT_GPIO_DEV_DEFINE)
