/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_altersense_rule_board_version

#include <altersense/altersense_rule.h>

#include "cros_board_info.h"

static uint32_t board_version;

struct altersense_rule_board_version_cfg {
	uint32_t mask;
	uint32_t min;
	uint32_t max;
};

static bool altersense_board_version_predicate(const struct device *dev,
					       const struct device *secondary)
{
	struct altersense_rule_board_version_cfg *cfg = dev->cfg;
	uint32_t version = board_version & cfg->mask;
	ARG_UNUSED(secondary);

	return (cfg->min <= version) && (version <= cfg->max);
}

static const struct altersense_rule_api altersense_rule_board_version_api = {
	.predicate = altersense_board_version_predicate,
};

static int altersense_rule_board_version_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	return cbi_get_board_version(&board_version);
}

#define INIT(n)                                                            \
	static struct altersense_rule_board_version_cfg cfg_##n = {        \
		.mask = DT_INST_PROP_OR(n, mask, 0xffffffff),              \
		.min = DT_INST_PROP_BY_IDX(n, range, 0),                   \
		.max = DT_INST_PROP_BY_IDX(n, range, 1),                   \
	};                                                                 \
	DEVICE_DT_INST_DEFINE(n, altersense_rule_board_version_init, NULL, \
			      NULL, &cfg_##n, POST_KERNEL, 10,             \
			      &altersense_rule_board_version_api);

DT_INST_FOREACH_STATUS_OKAY(INIT)