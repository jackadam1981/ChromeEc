/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __INTEL_RVP_BOARD_ID_H
#define __INTEL_RVP_BOARD_ID_H

#include <devicetree.h>
#include <drivers/gpio.h>

#define RVP_ID_GPIO_DT_SPEC_GET(idx, node_id, prop) \
	GPIO_DT_SPEC_GET_BY_IDX(node_id, prop, idx),

#define RVP_ID_CONFIG_LIST(node_id, prop)           \
	LISTIFY(DT_PROP_LEN(node_id, prop),         \
	RVP_ID_GPIO_DT_SPEC_GET, (), node_id, prop)

extern const struct gpio_dt_spec bom_id_config[];

extern const struct gpio_dt_spec fab_id_config[];

extern const struct gpio_dt_spec board_id_config[];

#endif /* __INTEL_RVP_BOARD_ID_H */
