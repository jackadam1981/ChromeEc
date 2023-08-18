/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charger.h"
#include "console.h"
#include "driver/charger/sm5803.h"
#include "extpower.h"
#include "hooks.h"
#include "usb_pd.h"

#include <zephyr/logging/log.h>

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ##args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ##args)

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

int extpower_is_present(void)
{
	int port;
	int rv;
	bool acok;

	for (port = 0; port < board_get_usb_pd_port_count(); port++) {
		rv = sm5803_is_acok(port, &acok);
		if ((rv == EC_SUCCESS) && acok)
			return 1;
	}

	return 0;
}

/*
 * Joxer not have a GPIO indicating whether extpower is present,
 * so detect using the charger(s).
 */
__override void board_check_extpower(void)
{
	static int last_extpower_present;
	int extpower_present = extpower_is_present();

	if (last_extpower_present ^ extpower_present)
		extpower_handle_update(extpower_present);

	last_extpower_present = extpower_present;
}

__override void board_hibernate(void)
{
	/* Shut down the chargers */
	if (board_get_usb_pd_port_count() == 2)
		sm5803_hibernate(CHARGER_SECONDARY);
	sm5803_hibernate(CHARGER_PRIMARY);
	LOG_INF("Charger(s) hibernated");
	cflush();
}

static inline enum ec_error_list chg_write8(int chgnum, int offset, int value)
{
	return i2c_write8(chg_chips[chgnum].i2c_port,
			  chg_chips[chgnum].i2c_addr_flags, offset, value);
}

static inline enum ec_error_list chg_read8(int chgnum, int offset, int *value)
{
	return i2c_read8(chg_chips[chgnum].i2c_port,
			 chg_chips[chgnum].i2c_addr_flags, offset, value);
}

static void charger_init(void)
{
	enum ec_error_list rv = EC_SUCCESS;
	int reg;
	int chip;

	for (chip = 0; chip < board_get_charger_chip_count(); chip++) {
		/* Set PHOT_DURATION to 10ms */
		rv |= chg_read8(chip, SM5803_REG_PHOT1, &reg);
		reg &= ~SM5803_PHOT1_DURATION;
		reg |= SM5803_PHOT1_DURATION_10ms
		       << SM5803_PHOT1_DURATION_SHIFT;
		rv |= chg_write8(chip, SM5803_REG_PHOT1, reg);

		/* Set VBUS_MONITOR_SEL to 4V */
		rv |= chg_read8(chip, SM5803_REG_PHOT2, &reg);
		reg &= ~SM5803_PROT2_VBUS_SEL;
		reg |= SM5803_PROT2_VBUS_SEL_4V;
		rv |= chg_write8(chip, SM5803_REG_PHOT2, reg);

		/* Set VSYS_MONITOR_SEL to 6V */
		rv |= chg_read8(chip, SM5803_REG_PHOT3, &reg);
		reg &= ~SM5803_PROT3_VSYS_SEL;
		reg |= SM5803_PROT3_VSYS_SEL_6V;
		rv |= chg_write8(chip, SM5803_REG_PHOT3, reg);

		/* Set IBAT_PHOT_SEL to 4.8A */
		rv |= chg_read8(chip, SM5803_REG_PHOT4, &reg);
		reg &= ~SM5803_PROT4_IBAT_SEL;
		reg |= SM5803_PROT4_IBAT_SEL_4P8A;
		rv |= chg_write8(chip, SM5803_REG_PHOT4, reg);

		if (rv)
			CPRINTS("%s %d: Failed initialization", CHARGER_NAME,
				chip);
	}
}
DECLARE_HOOK(HOOK_INIT, charger_init, HOOK_PRIO_DEFAULT);
