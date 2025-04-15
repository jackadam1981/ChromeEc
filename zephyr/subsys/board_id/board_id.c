#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

#include "system.h"

LOG_MODULE_DECLARE(board_id, LOG_LEVEL_INF);

#define DT_DRV_COMPAT intel_board_id

enum init_level {
	INIT_LEVEL_EARLY = 0,
	INIT_LEVEL_PRE_KERNEL_1,
	INIT_LEVEL_PRE_KERNEL_2,
	INIT_LEVEL_POST_KERNEL,
	INIT_LEVEL_APPLICATION,
#ifdef CONFIG_SMP
	INIT_LEVEL_SMP,
#endif /* CONFIG_SMP */
};

struct board_id_config {
	int board_version;
	int devices_count;
	const struct device **devices;
};

extern const struct init_entry __init_start[];
extern const struct init_entry __init_EARLY_start[];
extern const struct init_entry __init_PRE_KERNEL_1_start[];
extern const struct init_entry __init_PRE_KERNEL_2_start[];
extern const struct init_entry __init_POST_KERNEL_start[];
extern const struct init_entry __init_APPLICATION_start[];
extern const struct init_entry __init_end[];

static const struct init_entry *levels[] = {
	__init_EARLY_start,
	__init_PRE_KERNEL_1_start,
	__init_PRE_KERNEL_2_start,
	__init_POST_KERNEL_start,
	__init_APPLICATION_start,
#ifdef CONFIG_SMP
	__init_SMP_start,
#endif /* CONFIG_SMP */
	/* End marker */
	__init_end,
};


#define BOARD_ID_DEVICES_CHECK_(device) \
	BUILD_ASSERT(1 == DT_PROP(device, zephyr_deferred_init), \
	"All board-id devices must have `zephyr-deferred-init`");

#define BOARD_ID_DEVICES_CHECK(node_id, prop, idx)         \
	BOARD_ID_DEVICES_CHECK_(            \
		DT_CAT5(node_id, _P_, prop, _IDX_, idx))

#define BOARD_ID_INST_DEVICES_CHECK(inst)                          \
	DT_INST_FOREACH_PROP_ELEM(inst, devices,                  \
				      BOARD_ID_DEVICES_CHECK)

#define BOARD_ID_INST_DEVICES_COND_CHECK(inst)     \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, devices), \
		(BOARD_ID_INST_DEVICES_CHECK(inst)), ())

DT_INST_FOREACH_STATUS_OKAY(BOARD_ID_INST_DEVICES_COND_CHECK)

#define BOARD_ID_DEVICES_DEFINE_WITH_COMA(device) DEVICE_DT_GET(device),

#define BOARD_ID_DEVICES_DEFINE(node_id, prop, idx)         \
	BOARD_ID_DEVICES_DEFINE_WITH_COMA(            \
		DT_CAT5(node_id, _P_, prop, _IDX_, idx))

#define BOARD_ID_DEVICES_ARRAY_DEFINE(inst)                          \
	static const struct device *devices_##inst[] = {                  \
	DT_INST_FOREACH_PROP_ELEM(inst, devices,                  \
				      BOARD_ID_DEVICES_DEFINE) };

#define BOARD_ID_DEVICES_ARRAY_COND_DEFINE(inst) \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, devices), \
		(BOARD_ID_DEVICES_ARRAY_DEFINE(inst)), ())

DT_INST_FOREACH_STATUS_OKAY(BOARD_ID_DEVICES_ARRAY_COND_DEFINE)

static int board_id_init(enum init_level level, const struct device *dev)
{
	const struct init_entry *entry;
	const struct board_id_config * const config = dev->config;

	if (system_get_board_version() != config->board_version) {
		return 0;
	}


	for (int i = 0; i < config->devices_count; i++) {
		for (entry = levels[level]; entry < levels[level+1]; entry++) {
			if (config->devices[i] == entry->dev ) {
				LOG_INF("  Init %s", entry->dev->name);
				device_init(config->devices[i]);
			}
		}
	}
	return 0;
}

#define BOARD_ID_INST_DEFINE(inst)              \
	const struct board_id_config board_id_config_##inst = {    \
		.board_version = DT_INST_PROP(inst, board_version), \
		COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, devices), \
			(.devices_count = DT_INST_PROP_LEN(inst, devices),\
			.devices = devices_##inst,),   \
			(.devices_count = 0,      \
			.devices = NULL)) \
	};                                                   \
	DEVICE_DT_INST_DEFINE(inst, NULL, NULL, NULL,  \
		&board_id_config_##inst, POST_KERNEL, 100, NULL);

DT_INST_FOREACH_STATUS_OKAY(BOARD_ID_INST_DEFINE)

#define BOARD_ID_INST_GET_DEVICE(inst)   \
	DEVICE_DT_INST_GET(inst),

const struct device *devices[] = {
	DT_INST_FOREACH_STATUS_OKAY(BOARD_ID_INST_GET_DEVICE)
};

static int board_id_init_post_kernel(void)
{
	LOG_INF("%s", __func__);
	for (int i = 0; i < ARRAY_SIZE(devices); i++) {
		board_id_init(INIT_LEVEL_POST_KERNEL, devices[i]);
	}
	return 0;
}
SYS_INIT(board_id_init_post_kernel, POST_KERNEL, 101);

static int board_id_init_application(void)
{
	LOG_INF("%s", __func__);
	for (int i = 0; i < ARRAY_SIZE(devices); i++) {
		board_id_init(INIT_LEVEL_APPLICATION, devices[i]);
	}
	return 0;
}
SYS_INIT(board_id_init_application, APPLICATION, 101);
