/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PI3USB9201 USB BC 1.2 Charger Detector driver. */

#include "pi3usb9201.h"
#include "charge_manager.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "power.h"
#include "task.h"
#include "tcpm.h"
#include "timer.h"
#include "usb_charge.h"
#include "usb_pd.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

struct bc12_status {
	enum charge_supplier supplier;
	int current_limit;
};

static const struct bc12_status bc12_chg_limits[] = {
	{CHARGE_SUPPLIER_OTHER, 500},
	{CHARGE_SUPPLIER_PROPRIETARY, 2400},
	{CHARGE_SUPPLIER_PROPRIETARY, 2000},
	{CHARGE_SUPPLIER_PROPRIETARY, 1000},
	{CHARGE_SUPPLIER_NONE, 0},
	{CHARGE_SUPPLIER_BC12_CDP, 1500},
	{CHARGE_SUPPLIER_BC12_SDP, 500},
#if defined(CONFIG_CHARGE_RAMP_SW) || defined(CONFIG_CHARGE_RAMP_HW)
	/*
	 * If ramping is supported, then for DCP set the current limit to be the
	 * max supported for the port by the board. This because for DCP the
	 * charger is allowed to set its own max up to 5A.
	 */
	{CHARGE_SUPPLIER_BC12_DCP, PD_MAX_CURRENT_MA},
#else
	{CHARGE_SUPPLIER_BC12_DCP, 500},
#endif
};

static inline int raw_read8(int port, int offset, int *value)
{
	return i2c_read8(bc12_chips[port].i2c_port, bc12_chips[port].i2c_addr,
			 offset, value);
}

static inline int raw_write8(int port, int offset, int value)
{
	return i2c_write8(bc12_chips[port].i2c_port, bc12_chips[port].i2c_addr,
			  offset, value);
}

static int pi3usb9201_interrupt_mask(int port, int enable)
{
	int rv;
	int ctrl_reg;

	rv = raw_read8(port, PI3USB9201_REG_CTRL_1, &ctrl_reg);
	if (rv)
		return rv;
	if (enable)
		ctrl_reg |= PI3USB9201_REG_CTRL_1_INT_MASK;
	else
		ctrl_reg &= ~PI3USB9201_REG_CTRL_1_INT_MASK;

	return raw_write8(port, PI3USB9201_REG_CTRL_1, ctrl_reg);
}

static int pi3usb9201_bc12_detect_ctrl(int port, int on)
{
	int ctrl_reg;
	int rv;

	rv = raw_read8(port, PI3USB9201_REG_CTRL_2, &ctrl_reg);
	if (rv)
		return rv;
	if (on)
		ctrl_reg |= PI3USB9201_REG_CTRL_2_START_DET;
	else
		ctrl_reg &= ~PI3USB9201_REG_CTRL_2_START_DET;

	return raw_write8(port, PI3USB9201_REG_CTRL_2, ctrl_reg);
}

static int pi3usb9201_set_mode(int port, int desired_mode)
{
	int rv;
	int reg;

	rv = raw_read8(port, PI3USB9201_REG_CTRL_1, &reg);
	if (rv)
		return rv;

	reg &= ~(PI3USB9201_REG_CTRL_1_INT_MODE_MASK <<
		 PI3USB9201_REG_CTRL_1_INT_MODE_BIT_SHIFT);
	reg |= (desired_mode << PI3USB9201_REG_CTRL_1_INT_MODE_BIT_SHIFT);
	rv = raw_write8(port, PI3USB9201_REG_CTRL_1, reg);

	return  raw_write8(port, PI3USB9201_REG_CTRL_1, reg);
}

static void bc12_update_charge_manager(int port)
{
	int client_status;
	int rv;
	int index;
	struct charge_port_info new_chg;
	enum charge_supplier supplier;

	/* Set charge voltage to 5V */
	new_chg.voltage = USB_CHARGER_VOLTAGE_MV;
	/* Read BC 1.2 client mode detection result */
	rv = raw_read8(port, PI3USB9201_REG_CLIENT_STS, &client_status);
	/* Find set bit position. If no bits set will return 0 */
	index = __builtin_ffs(client_status) - 1;

	if (!rv && (index >= 0)) {
		new_chg.current = bc12_chg_limits[index].current_limit;
		supplier = bc12_chg_limits[index].supplier;
	} else {
		/*
		 * If bc 1.2 detetion failed for some reason, then set limit to
		 * minimum and set suppiler to other.
		 */
		new_chg.current = 500;
		supplier = CHARGE_SUPPLIER_OTHER;
	}
	CPRINTS("pi3usb9201[p%d]: sts = 0x%x, lim = %d mA, supplier = %d",
		port, client_status, new_chg.current, supplier);
	/* bc1.2 is complete and start bit does not auto clear */
	pi3usb9201_bc12_detect_ctrl(port, 0);
	/* Inform charge manager of new supplier type and current limit */
	charge_manager_update_charge(supplier, port, &new_chg);
}

static void bc12_detect_start(int port)
{
	/* Put pi3usb9201 into client mode */
	pi3usb9201_set_mode(port, PI3USB9201_CLIENT_MODE);
	/* Have pi3usb9201 start bc1.2 detection */
	pi3usb9201_bc12_detect_ctrl(port, 1);
	/* Unmask interrupt to wake task when detection completes */
	pi3usb9201_interrupt_mask(port, 0);
}

static void bc12_power_down(int port)
{
	/* Put pi3usb9201 into its power down mode */
	pi3usb9201_set_mode(port, PI3USB9201_POWER_DOWN);
	/* The start bc1.2 bit does not auto clear */
	pi3usb9201_bc12_detect_ctrl(port, 0);
	/* Mask interrupts unitl next bc1.2 detection event */
	pi3usb9201_interrupt_mask(port, 1);
	/* Let charge manager know there's no more charge available. */
	charge_manager_update_charge(CHARGE_SUPPLIER_NONE, port, NULL);
}

static void bc12_handle_vbus_change(int port)
{
	int role = pd_get_role(port);
	int vbus = pd_snk_is_vbus_provided(port);

#ifndef CONFIG_USB_PD_VBUS_DETECT_TCPC
	CPRINTS("VBUS p%d %d", port, vbus);
#endif
	/*
	 * TODO(b/124061702): For host mode, currently only setting it to host
	 * CDP mode. However, there are 3 host status bits to know things such
	 * as an adpater connected, but no USB device present, or bc1.2 activity
	 * detected.
	 */
	if (role == PD_ROLE_SOURCE &&
	    board_vbus_source_enabled(port))
		/*
		 * For source role, if vbus is present, then set
		 * CDP host mode (will close D+/D-) switches. If
		 * vbus is not present, then put into power down
		 * mode.
		 */
		pi3usb9201_set_mode(port,
				    PI3USB9201_CDP_HOST_MODE);
	else if (pd_snk_is_vbus_provided(port))
		bc12_detect_start(port);
	else
		bc12_power_down(port);
}

void usb_charger_task(void *u)
{
	int port = (task_get_current() == TASK_ID_USB_CHG_P0 ? 0 : 1);
	uint32_t evt;

	/*
	 * The is no specific initialization required for the pi3usb9201 other
	 * than enabling the interrupt mask.
	 */
	pi3usb9201_interrupt_mask(port, 1);
	/*
	 * Upon EC reset, there likely won't be a VBUS status change to set the
	 * event to wake up this task. So need to check if VBUS is
	 * present to make sure that D+/D- switches remain in the correct state
	 * and bc1.2 detection is triggered (for client mode) to set the correct
	 * input current limit.
	 */
	bc12_handle_vbus_change(port);

	while (1) {
		/* Wait for interrupt */
		evt = task_wait_event(-1);

		/* Interrupt from the Pericom chip, determine charger type */
		if (evt & USB_CHG_EVENT_BC12)
			bc12_update_charge_manager(port);

		if (evt & USB_CHG_EVENT_VBUS)
			bc12_handle_vbus_change(port);
	}
}

void usb_charger_set_switches(int port, enum usb_switch setting)
{
	/*
	 * Switches are controlled automatically based on whether the port is
	 * acting as a source or as sink and the result of BC1.2 detection.
	 */
}

#if defined(CONFIG_CHARGE_RAMP_SW) || defined(CONFIG_CHARGE_RAMP_HW)
int usb_charger_ramp_allowed(int supplier)
{
	/* Don't allow ramp if charge supplier is OTHER or SDP */
	return !(supplier == CHARGE_SUPPLIER_OTHER ||
		 supplier == CHARGE_SUPPLIER_BC12_SDP);
}

int usb_charger_ramp_max(int supplier, int sup_curr)
{
	/*
	 * Use the level from the bc12_chg_limits table above except for
	 * proprietary of CDP and in those cases the charge current from the
	 * charge manager is already set at the max determined by bc1.2
	 * detection.
	 */
	switch (supplier) {
	case CHARGE_SUPPLIER_BC12_DCP:
		return PD_MAX_CURRENT_MA;
	case CHARGE_SUPPLIER_BC12_SDP:
		return 500;
	case CHARGE_SUPPLIER_BC12_CDP:
	case CHARGE_SUPPLIER_PROPRIETARY:
		return sup_curr;
	default:
		return 500;
	}
}
#endif /* CONFIG_CHARGE_RAMP_SW || CONFIG_CHARGE_RAMP_HW */
