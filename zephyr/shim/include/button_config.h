/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __BUTTON_CONFIG_H
#define __BUTTON_CONFIG_H

#include <zephyr/devicetree.h>

#define BUTTON_CFG_COMPAT cros_ec_button_cfg
#define DT_BUTTON_CFG_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(BUTTON_CFG_COMPAT)

#define BUTTON_CFG_ENUM(val) DT_CAT(BUTTON_CFG_, val)
#define BUTTON_CFG_TYPE(node) \
	BUTTON_CFG_ENUM(DT_STRING_UPPER_TOKEN(node, button_name)),

enum button_cfg_type {
#if DT_NODE_EXISTS(DT_BUTTON_CFG_NODE)
	DT_FOREACH_CHILD(DT_BUTTON_CFG_NODE, BUTTON_CFG_TYPE)
#endif
		BUTTON_CFG_ENUM(COUNT),
};

const struct button_config *get_button_cfg(enum button_cfg_type type);

#endif /* __BUTTON_CONFIG_H */
