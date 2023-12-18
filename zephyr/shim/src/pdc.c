/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drivers/pdc.h"
#include "pdc/pdc.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>

/* clang-format off */

/* Given a named-usbc-port, get the associated PDC and its attached I2C bus */
#define PDC_ENTRY_COMMA(id)                                                \
	[DT_PROP_BY_IDX(id, reg, 0)] = {                                   \
		.device = DEVICE_DT_GET(DT_PHANDLE(id, pdc)),              \
		.bus_info.i2c = I2C_DT_SPEC_GET(DT_PHANDLE(id, pdc)),      \
	},

/* Only consider named-usbc-ports with the pdc property */
#define REGISTER_PDC_DEVICE(usbc_id)                                       \
	COND_CODE_1(                                                       \
		DT_NODE_HAS_PROP(usbc_id, pdc),                            \
		(PDC_ENTRY_COMMA(usbc_id)),                                \
		()                                                         \
	)

/** Build an array of all PDC device pointers and their associated I2C bus
 *  under named-usbc-port, indexed by port number.
 */
struct pdc_config_t {
	const struct device *device;
	union {
		struct i2c_dt_spec i2c;
	} bus_info;
};

static const struct pdc_config_t pdc_config[] = {
	DT_FOREACH_STATUS_OKAY(named_usbc_port, REGISTER_PDC_DEVICE)
};

#define PDC_COUNT ARRAY_SIZE(pdc_config)

/* clang-format on */

const struct device *pdc_get_device_for_port(const int port)
{
	if (port < 0 || port >= PDC_COUNT) {
		return NULL;
	}

	return pdc_config[port].device;
}

int pdc_get_legacy_i2c_info_for_port(const int port, uint16_t *i2c_port,
				     uint16_t *i2c_addr)
{
	if (!i2c_port || !i2c_addr) {
		return -EINVAL;
	}

	if (port < 0 || port >= PDC_COUNT) {
		return -ERANGE;
	}

	int i2c_port_num =
		i2c_get_port_from_device(pdc_config[port].bus_info.i2c.bus);

	if (i2c_port_num < 0) {
		/* No named-i2c-bus matches the device this PDC is on */
		return -EINVAL;
	}

	*i2c_port = i2c_port_num;
	*i2c_addr = pdc_config[port].bus_info.i2c.addr;

	return 0;
}

const size_t pdc_get_device_count(void)
{
	return PDC_COUNT;
}
