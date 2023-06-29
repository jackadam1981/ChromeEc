/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "common.h"
#include "cros_cbi.h"
#include "driver/retimer/ps8811.h"
#include "hooks.h"
#include "i2c/i2c.h"
#include "system.h"
#include "usb_mux_config.h"

#include <ap_power/ap_power.h>

#define CPRINTSUSB(format, args...) cprints(CC_USB, format, ##args)
#define CPRINTFUSB(format, args...) cprintf(CC_USB, format, ##args)

struct ps8811_reg_val {
	uint8_t reg;
	uint16_t val;
};

const static struct ps8811_reg_val equalizer_table[] = {
	{
		/* Set channel B EQ setting */
		.reg = PS8811_REG1_USB_BEQ_LEVEL,
		.val = (PS8811_BEQ_I2C_LEVEL_UP_13DB
			<< PS8811_BEQ_I2C_LEVEL_UP_SHIFT) |
		       (PS8811_BEQ_PIN_LEVEL_UP_18DB
			<< PS8811_BEQ_PIN_LEVEL_UP_SHIFT),
	},
	{
		/* Set BDE pin setting */
		.reg = PS8811_REG1_USB_BDE_CONFIG,
		.val = (PS8811_BDE_PIN_MID_LEVEL_3DB
			<< PS8811_BDE_PIN_MID_LEVEL_SHIFT) |
		       PS8811_BEQ_CONFIG_REG_ENABLE |
		       PS8811_BEQ_ADAPTIVE_REG_ENABLE,
	},
};

#define NUM_EQ_WWAN_ARRAY ARRAY_SIZE(equalizer_table)

/* USB-A ports */
enum usba_port { USBA_PORT_A0, USBA_PORT_COUNT };
const static struct usb_mux usba_ps8811[] = {
	[USBA_PORT_A0] = {
		.usb_port = USBA_PORT_A0,
		.i2c_port = I2C_PORT_NODELABEL(i2c6_1),
		.i2c_addr_flags = PS8811_I2C_ADDR_FLAGS1,
	},
};

static int usba_retimer_ps8811_tuning(int port)
{
	int rv;
	int val;
	int i;
	const struct usb_mux *me = &usba_ps8811[port];

	rv = ps8811_i2c_read(me, PS8811_REG_PAGE1, PS8811_REG1_USB_BEQ_LEVEL,
			     &val);

	if (rv) {
		CPRINTSUSB("A0: PS8811 retimer response fail! : %d", rv);
	}

	if (chipset_in_state(CHIPSET_STATE_ON)) {
		for (i = 0; i < NUM_EQ_WWAN_ARRAY; i++) {
			rv |= ps8811_i2c_write(me, PS8811_REG_PAGE1,
					       equalizer_table[i].reg,
					       equalizer_table[i].val);
		}

		/* Set channel A output swing */
		rv |= ps8811_i2c_field_update(me, PS8811_REG_PAGE1,
					      PS8811_REG1_USB_CHAN_A_SWING,
					      PS8811_CHAN_A_SWING_MASK,
					      0x2 << PS8811_CHAN_A_SWING_SHIFT);
	}
	return rv;
}

extern void baseboard_a1_retimer_resetup(void);

void baseboard_a1_retimer_setup(void)
{
	int i, rv;

	if (usb_db_type == FW_USB_DB_USB4_ANX7452_V2) {
		for (i = 0; i < USBA_PORT_COUNT; ++i)
			rv = usba_retimer_ps8811_tuning(i);
		if (rv != EC_SUCCESS)
			baseboard_a1_retimer_resetup();
	}
}
DECLARE_DEFERRED(baseboard_a1_retimer_setup);

void baseboard_a1_retimer_resetup(void)
{
	hook_call_deferred(&baseboard_a1_retimer_setup_data, 3000 * MSEC);
}

__override void board_resume_change(struct ap_power_ev_callback *cb,
				    struct ap_power_ev_data data)
{
	switch (data.event) {
	default:
		return;

	case AP_POWER_RESUME:
		/* Any retimer tuning can be done after the retimer turns on */
		hook_call_deferred(&baseboard_a1_retimer_setup_data,
				   10000 * MSEC);
		break;
	}
}
