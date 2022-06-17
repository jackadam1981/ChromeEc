/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_INCLUDE_EMUL_EMUL_VBUS_H_
#define ZEPHYR_INCLUDE_EMUL_EMUL_VBUS_H_

#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
/* TODO: Better header? */
#include <zephyr/sys/util_internal.h>
#include <zephyr/toolchain/common.h>

struct vbus_node {
	void (*on_changed)(const struct emul *vbus_parent,
			   const struct emul *dev_emul, int old_voltage_mv,
			   int old_current_ma, int new_voltage_mv,
			   int new_current_ma);
	/* Corresponding to VBUS wire */
	const struct emul *const vbus_emul;
	/* Corresponding to the emulator that is "connected" to this VBUS wire
	 */
	const struct emul *const dev_emul;
};

extern const struct vbus_node __vbus_node_list_start[];
extern const struct vbus_node __vbus_node_list_end[];

#define VBUS_NODE(drv_inst, on_changed_cb)                            \
	static STRUCT_SECTION_ITERABLE(vbus_node, UTIL_CAT(vbus_node_, drv_inst)) = {        \
		.on_changed = on_changed_cb,                          \
		.vbus_emul = EMUL_DT_GET(DT_PHANDLE(drv_inst, vbus)), \
		.dev_emul = EMUL_DT_GET(drv_inst),                    \
	};

int vbus_emul_set(const struct emul *vbus_parent, int voltage_mv,
		  int current_ma);

#endif /* ZEPHYR_INCLUDE_EMUL_EMUL_VBUS_H_ */
