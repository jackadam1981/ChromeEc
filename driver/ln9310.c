/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * LION Semiconductor LN-9310 switched capacitor converter.
 */

#include "common.h"
#include "console.h"
#include "ln9310.h"
#include "hooks.h"
#include "i2c.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

static int power_good;

int ln9310_power_good(void)
{
	return power_good;
}

static inline int raw_read8(int offset, int *value)
{
	return i2c_read8(ln9310_config.i2c_port,
			 ln9310_config.i2c_addr_flags,
			 offset,
			 value);
}

static inline int field_update8(int offset, int mask, int value)
{
	/* Clear mask and then set value in i2c reg value */
	return i2c_field_update8(ln9310_config.i2c_port,
				 ln9310_config.i2c_addr_flags,
				 offset,
				 mask,
				 value);
}

static void ln9310_irq_deferred(void)
{
	int status, val, power_good_2to1, power_good_3to1;

	/* Check if the device is active in 2:1 switching mode */
	status = raw_read8(LN9310_REG_INT1, &val);
	if (status) {
		CPRINTS("LN9310 reading INT1 failed");
		return;
	}

	CPRINTS("LN9310 received interrupt: 0x%x", val);
	/* Don't care other interrupts except mode change */
	if (!(val & LN9310_INT1_MODE))
		return;

	/* Check if the device is active in 2:1 switching mode */
	status = raw_read8(LN9310_REG_SYS_STS, &val);
	if (status) {
		CPRINTS("LN9310 reading SYS_STS failed");
		return;
	}
	CPRINTS("LN9310 system status: 0x%x", val);
  
	// 2:1 PGOOD
	power_good_2to1 = !!(val & LN9310_SYS_SWITCHING21_ACTIVE);
	// 3:1 PGOOD
	power_good_3to1 = !!(val & LN9310_SYS_SWITCHING31_ACTIVE);

	power_good = power_good_2to1 || power_good_3to1;
	
}
DECLARE_DEFERRED(ln9310_irq_deferred);

void ln9310_interrupt(enum gpio_signal signal)
{
	hook_call_deferred(&ln9310_irq_deferred_data, 0);
}

void ln9310_init(void)
{
	int status, val, vin_too_high;

	/* Check if Input is too high*/
	CPRINTS("LN9310 Checking input Voltage (threshold=10V)");
  
  	/* Turn on INFET_OUT_SWITCH_OK comparator */
  	/*         Configure INFET_OUT_SWITCH_OK to 10V   */
  	field_update8(LN9310_REG_TRACK_CTRL,
		      LN9310_TRACK_INFET_OUT_SWITCH_OK_EN_MASK |
                      		LN9310_TRACK_INFET_OUT_SWITCH_OK_CFG_MASK,
		      LN9310_TRACK_INFET_OUT_SWITCH_OK_EN_ON |
                     		LN9310_TRACK_INFET_OUT_SWITCH_OK_CFG_10V);
  
	/* Read INFET_OUT_SWITCH_OK comparator */
	status = raw_read8(LN9310_REG_BC_STS_B, &val);
	if (status) {
		CPRINTS("LN9310 reading BC_STS_B failed");
		return;
	}
	CPRINTS("LN9310 BC_STS_B: 0x%x", val);

	/* If INFET_OUT_SWITCH_OK=0, VIN < 10V --> Valid range for 2:1 op
	   If INFET_OUT_SWITCH_OK=1, VIN > 10V --> Too high */
	vin_too_high = !!(val & LN9310_BC_STS_B_INFET_OUT_SWITCH_OK);
	CPRINTS("LN9310 3S Battery Detection: 0x%x", vin_too_high);

	/* Turn off INFET_OUT_SWITCH_OK comparator */
  	field_update8(LN9310_REG_TRACK_CTRL,
		      LN9310_TRACK_INFET_OUT_SWITCH_OK_EN_MASK,
		      LN9310_TRACK_INFET_OUT_SWITCH_OK_EN_OFF);
  	CPRINTS("LN9310 INFET_OUT_SWITCH_OK Comparator turned off");

  	/* Set OPERATION_MODE update method 
  	    - OP_MODE_MANUAL_UPDATE = 0
  	    - OP_MODE_SELF_SYNC_EN  = 1 */
  	field_update8(LN9310_REG_TRACK_CTRL,
		      LN9310_PWR_OP_MODE_MANUAL_UPDATE_MASK,
		      LN9310_PWR_OP_MODE_MANUAL_UPDATE_OFF);

  	field_update8(LN9310_REG_TIMER_CTRL,
		      LN9310_TIMER_OP_SELF_SYNC_EN_MASK,
		      LN9310_TIMER_OP_SELF_SYNC_EN_ON);

  	usleep(LN9310_CDC_DELAY);
  	CPRINTS("LN9310 OP_MODE Update method: Self-sync");
  	

	if(vin_too_high) {
		CPRINTS("LN9310 init stopped. Input voltage is too high");
	}
	else {
		CPRINTS("LN9310 init (2:1 operation)");

		/* Enable track protection and SC_OUT configs for 2:1 switching  */
		field_update8(LN9310_REG_MODE_CHANGE_CFG,
			      LN9310_MODE_TM_TRACK_MASK |
					LN9310_MODE_TM_SC_OUT_PRECHG_MASK,
			      LN9310_MODE_TM_TRACK_SWITCH21 |
					LN9310_MODE_TM_SC_OUT_PRECHG_SWITCH21);
	  
		/* Enable 2:1 operation mode */
		field_update8(LN9310_REG_PWR_CTRL,
			      LN9310_PWR_OP_MODE_MASK,
			      LN9310_PWR_OP_MODE_SWITCH21);
		
	  	/* 2S Lower bounde Delta configurations */
		field_update8(LN9310_REG_SYS_CTRL,
			      LN9310_SYS_CTRL_LB_DELTA_MASK,
			      LN9310_SYS_CTRL_LB_DELTA_2S);
	  
	}

	/* Unmask the MODE change interrupt */
	field_update8(LN9310_REG_INT1_MSK,
			      LN9310_INT1_MODE,
			      0);
	  
	/* Dummy Clear all interrupts */
	status = raw_read8(LN9310_REG_INT1, &val);
	if (status) {
		CPRINTS("LN9310 reading INT1 failed");
		return;
	}

	CPRINTS("LN9310 cleared interrupts: 0x%x", val);
}
DECLARE_HOOK(HOOK_INIT, ln9310_init, HOOK_PRIO_DEFAULT);
