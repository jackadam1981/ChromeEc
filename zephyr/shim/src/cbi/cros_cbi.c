/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cbi_transfer.h"
#include "cros_board_info.h"
#include "cros_cbi.h"

#include <zephyr/init.h>

static int cros_cbi_ec_init(void)
{
	if (IS_ENABLED(CONFIG_PLATFORM_EC_CBI_TRANSFER_EEPROM_FLASH)) {
		cros_cbi_transfer_eeprom_to_flash();
	}
	cros_cbi_ssfc_init();
	cros_cbi_fw_config_init();

	return 0;
}

SYS_INIT(cros_cbi_ec_init, POST_KERNEL,
	 CONFIG_PLATFORM_EC_CBI_SYS_INIT_PRIORITY);

#ifdef CONFIG_PLATFORM_EC_CBI_EEPROM
/* When using EEPROM-based CBI, we depend on the underlying I2C and EEPROM
 * drivers to init first. Enforce this order for all cases (EEPROM vs flash vs
 * both) for the sake of consistency
 */
BUILD_ASSERT(CONFIG_I2C_INIT_PRIORITY <
	     CONFIG_PLATFORM_EC_CBI_SYS_INIT_PRIORITY);
BUILD_ASSERT(CONFIG_EEPROM_AT2X_INIT_PRIORITY <
	     CONFIG_PLATFORM_EC_CBI_SYS_INIT_PRIORITY);
#endif /* defined(CONFIG_PLATFORM_EC_CBI_EEPROM) */

#ifdef CONFIG_PDC_RUNTIME_PORT_CONFIG
/* CBI must be accessible before charge manager and the PDC subsystem when
 * using runtime port configuration so board-specific data can be considered.
 */
BUILD_ASSERT(CONFIG_PLATFORM_EC_CBI_SYS_INIT_PRIORITY <
	     CONFIG_CHARGE_MANAGER_SYS_INIT_PRIORITY);
#endif /* defined(CONFIG_PDC_RUNTIME_PORT_CONFIG) */
