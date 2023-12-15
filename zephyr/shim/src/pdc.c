/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drivers/pdc.h"
#include "pdc/pdc.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

/* clang-format off */

#define PDC_ENTRY_COMMA(id)                                                \
	[DT_PROP_BY_IDX(id, reg, 0)] = DEVICE_DT_GET(DT_PHANDLE(id, pdc)),

#define REGISTER_PDC_DEVICE(usbc_id)                                       \
	COND_CODE_1(                                                       \
		DT_NODE_HAS_PROP(usbc_id, pdc),                            \
		(PDC_ENTRY_COMMA(usbc_id)),                                \
		()                                                         \
	)

/** Build an array of all PDC device pointers under named-usbc-port indexed
 *  by port number.
 */
static const struct device *pdc_devices[] = {
	DT_FOREACH_STATUS_OKAY(named_usbc_port, REGISTER_PDC_DEVICE)
};

#define PDC_COUNT ARRAY_SIZE(pdc_devices)

/* clang-format on */

const struct device *pdc_get_device_for_port(const int port)
{
	if (port < 0 || port >= PDC_COUNT) {
		return NULL;
	}

	return pdc_devices[port];
}

const size_t pdc_get_device_count(void)
{
	return PDC_COUNT;
}
