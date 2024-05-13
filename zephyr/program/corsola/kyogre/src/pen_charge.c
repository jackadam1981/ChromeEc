/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "gpio/gpio_int.h"
#include "hooks.h"

#include <dt-bindings/gpio_defines.h>

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)

test_export_static enum {
	STATUS_UNKNOWN, /* ERR/CHG/STP_TIME have not been started yet */
	STATUS_ERROR, /* Stopped charging for ERR_TIME */
	STATUS_CHARGE, /* Started Charging for CHG_TIME */
	STATUS_STOP, /* Stopped charging for STP_TIME */
} pen_charge_status = STATUS_UNKNOWN;

test_export_static volatile int pen_chg_time = 0;
test_export_static volatile int pen_stp_time = 0;
test_export_static volatile int pen_err_time = 0;

#define CHG_TIME 43200 /* 12 hours */
#define STP_TIME 10 /* 10 seconds */
#define ERR_TIME 600 /* 10 minutes */

test_export_static uint8_t flags;
#define PEN_FAULT_DETECT BIT(0)

/*
 * Pen charge is controlled by EC
 *
 * 1) Fail safe:
 *   When pen fault is detected, pen charge will be
 *   stopped for 10 minutes [ERR_TIME].
 *
 *   |----Charge--|---Stop---|----Charge----|
 *                ^   10m
 *              fault
 *
 * 2) Repeted charge:
 *   To recover self discharge, pen charge will be
 *   restarted every 12 hours [CHG_TIME] with 10
 *   seconds rest [STP_TIME].
 *
 *   |----Charge----|-Stop-|----Charge----|-Stop-|
 *         12h        10s        12h        10s
 */
test_export_static void pen_charge(void)
{
	if (flags & PEN_FAULT_DETECT) {
		/* Set error timer */
		pen_err_time = ERR_TIME;
		pen_chg_time = 0;
		pen_stp_time = 0;
		flags &= ~PEN_FAULT_DETECT;
	}

	if (pen_err_time > 0) {
		/* Disalbe pen charge during counting error timer */
		pen_err_time--;
		pen_charge_status = STATUS_ERROR;
	} else if (pen_chg_time > 0) {
		/* Enalbe pen charge during counting charge timer */
		pen_chg_time--;
		pen_charge_status = STATUS_CHARGE;
	} else if (pen_stp_time > 0) {
		/* Disalbe pen charge during counting stop timer */
		pen_stp_time--;
		pen_charge_status = STATUS_STOP;
	}

	if ((pen_err_time == 0) && (pen_chg_time == 0) && (pen_stp_time == 0)) {
		/* All timers ware expired - Set initial values  */
		pen_err_time = 0;
		pen_chg_time = CHG_TIME;
		pen_stp_time = STP_TIME;
	}

	switch (pen_charge_status) {
	case STATUS_ERROR:
		gpio_pin_configure_dt(
			GPIO_DT_FROM_NODELABEL(ec_pen_chg_dis_odl),
			GPIO_ODR_LOW);
		break;
	case STATUS_CHARGE:
		gpio_pin_configure_dt(
			GPIO_DT_FROM_NODELABEL(ec_pen_chg_dis_odl),
			GPIO_ODR_HIGH);
		break;
	case STATUS_STOP:
		gpio_pin_configure_dt(
			GPIO_DT_FROM_NODELABEL(ec_pen_chg_dis_odl),
			GPIO_ODR_LOW);
		break;
	default:
		break;
	}
}
DECLARE_HOOK(HOOK_SECOND, pen_charge, HOOK_PRIO_DEFAULT);

static void board_pen_fault_init(void)
{
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_pen_fault));
}
DECLARE_HOOK(HOOK_INIT, board_pen_fault_init, HOOK_PRIO_DEFAULT);

test_mockable void pen_fault_interrupt(enum gpio_signal signal)
{
	if (signal == GPIO_SIGNAL(DT_NODELABEL(pen_fault_od)))
		/* This function sets PEN_FAULT_DETECT only.            */
		/* pen_charge() disables pen charge on next HOOK_SECOND */
		flags |= PEN_FAULT_DETECT;
}
