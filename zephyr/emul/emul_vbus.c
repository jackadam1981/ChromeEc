/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define LOG_LEVEL DEBUG
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(emul_vbus);

#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>

#include "emul/emul_isl923x.h"
#include "emul/emul_vbus.h"

int vbus_emul_set(const struct emul *vbus_parent, int voltage_mv,
		  int current_ma)
{
#if 0
	const struct emul *charger_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(isl923x_emul)));

	isl923x_emul_set_adc_vbus(charger_emul, voltage_mv);
#else
	for (const struct vbus_node *node = __vbus_node_list_start;
			node < __vbus_node_list_end; ++node) {
		if (node->vbus_emul == vbus_parent && node->on_changed) {
			/* TODO: Store old voltage and current somewhere. */
			node->on_changed(vbus_parent, node->dev_emul, 0, 0,
					voltage_mv, current_ma);
		}
	}
#endif

	/* TODO: Return based on whether voltage already set */
	return 0;
}
