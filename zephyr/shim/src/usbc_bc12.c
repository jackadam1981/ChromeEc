/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT usbc_bc12

#include <devicetree.h>
#include "bc12/pi3usb9201_public.h"
#include "usb_pd.h"

#if DT_NODE_EXISTS(DT_PATH(usbc, port_power))

#define POWER_PORT_NODE DT_PATH(usbc, port_power)

#if !DT_NODE_HAS_COMPAT(POWER_PORT_NODE, usbc_bc12)
#error "Invalid bc12 power node in device tree"
#endif

#define POWER_PORT_PHANDLE	DT_PHANDLE(POWER_PORT_NODE, port)
#define POWER_PORT		DT_STRING_UPPER_TOKEN(POWER_PORT_PHANDLE, \
								enum_name)
#define POWER_ADDR_FLAGS	DT_STRING_UPPER_TOKEN(POWER_PORT_NODE, \
								addr_flags)
#endif /* DT_NODE_EXISTS(DT_PATH(usbc, port_power)) */

#if DT_NODE_EXISTS(DT_PATH(usbc, port_eeprom))

#define EEPROM_PORT_NODE DT_PATH(usbc, port_eeprom)

#if !DT_NODE_HAS_COMPAT(EEPROM_PORT_NODE, usbc_bc12)
#error "Invalid bc12 eeprom node in device tree"
#endif

#define EEPROM_PORT_PHANDLE	DT_PHANDLE(EEPROM_PORT_NODE, port)
#define EEPROM_PORT		DT_STRING_UPPER_TOKEN(EEPROM_PORT_PHANDLE, \
								enum_name)
#define EEPROM_ADDR_FLAGS	DT_STRING_UPPER_TOKEN(EEPROM_PORT_NODE, \
								addr_flags)
#endif /* DT_NODE_EXISTS(DT_PATH(usbc, port_eeprom)) */

/* BC1.2 */
const struct pi3usb9201_config_t pi3usb9201_bc12_chips[] = {
	{
		.i2c_port = POWER_PORT,
		.i2c_addr_flags = POWER_ADDR_FLAGS,
	},
	{
		.i2c_port = EEPROM_PORT,
		.i2c_addr_flags = EEPROM_ADDR_FLAGS,
	},
};
