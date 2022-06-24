/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "include/button.h"
#include "button_config.h"

#define BUTTON_CFG_DEF(node)                           \
	{ .name = DT_PROP(node, button_name),          \
	  .type = DT_PROP_OR(node, button_type, 0),    \
	  .gpio = GPIO_SIGNAL(DT_PHANDLE(node, gpio)), \
	  .debounce_us = DT_PROP(node, debounce_us),   \
	  .flags = DT_PROP(node, flags) },

#if DT_NODE_EXISTS(DT_BUTTON_CFG_NODE)
static const struct button_config button_configs[] = { DT_FOREACH_CHILD(
	DT_BUTTON_CFG_NODE, BUTTON_CFG_DEF) };

const struct button_config *get_button_cfg(enum button_cfg_type type)
{
	const struct button_config *cfg = NULL;

	if (type < BUTTON_CFG_COUNT) {
		cfg = &button_configs[type];
	}

	return cfg;
}
#endif /* DT_NODE_EXISTS(DT_BUTTON_CFG_NODE) */
