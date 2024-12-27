/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charger.h"
#include "console.h"
#include "driver/charger/bq25710.h"
#include "driver/charger/bq257x0_regs.h"
#include "extpower.h"
#include "hooks.h"
#include "i2c.h"

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ##args)

static void set_bq25770_charge_option_5(void)
{
	int reg;
	int rv;

	rv = i2c_read16(chg_chips[0].i2c_port, chg_chips[0].i2c_addr_flags,
			BQ25770_REG_CHARGE_OPTION_5, &reg);
	if (rv) {
		CPRINTF("-- read BQ25770 0x19 fail!\n");
		return;
	}

	CPRINTF("-- read 0x%04x from 0x19\n", reg);

	/* Disable BATCOC */
	reg = SET_BQ_FIELD(BQ25770, CHARGE_OPTION_5, BATCOC_CONFIG,
			   BQ25770_CHARGE_OPTION_5_BATCOC_CONFIG__DISABLE, reg);

	/* TODO: change to recommand config */
	reg = SET_BQ_FIELD(BQ25770, CHARGE_OPTION_5, SINGLE_DUAL_TRANS_TH,
			   BQ25770_CHARGE_OPTION_5_SINGLE_DUAL_TRANS__7A, reg);

	rv = i2c_write16(chg_chips[0].i2c_port, chg_chips[0].i2c_addr_flags,
			 BQ25770_REG_CHARGE_OPTION_5, reg);
	CPRINTF("-- write 0x%04x to 0x19 %s\n", reg, (rv == 0) ? "ok" : "fail");
}

static void set_bq25770_auto_charge(void)
{
	int reg;
	int rv;

	rv = i2c_read16(chg_chips[0].i2c_port, chg_chips[0].i2c_addr_flags,
			BQ25770_REG_AUTO_CHARGE, &reg);
	if (rv) {
		CPRINTF("-- read BQ25770 0x1a fail!\n");
		return;
	}

	CPRINTF("-- read 0x%04x from 0x1a\n", reg);

	/* Set ACOV to 25v for 20V SPR */
	reg = SET_BQ_FIELD(BQ25770, AUTO_CHARGE, ACOV_ADJ,
			   BQ25770_AUTO_CHARGE_ACOV_ADJ__25V, reg);

	rv = i2c_write16(chg_chips[0].i2c_port, chg_chips[0].i2c_addr_flags,
			 BQ25770_REG_AUTO_CHARGE, reg);
	CPRINTF("-- write 0x%04x to 0x1a %s\n", reg, (rv == 0) ? "ok" : "fail");
}

static void set_bq25770_reg_custom(void)
{
	set_bq25770_charge_option_5();
	set_bq25770_auto_charge();
}
DECLARE_HOOK(HOOK_INIT, set_bq25770_reg_custom,
	     HOOK_PRIO_POST_BATTERY_INIT + 1);
