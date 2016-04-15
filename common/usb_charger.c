/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * USB charger / BC1.2 task. This code assumes that CONFIG_CHARGE_MANAGER
 * is defined and implemented.
 */

#include "charge_manager.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "task.h"
#include "timer.h"
#include "usb_charge.h"
#include "usb_pd.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

/* Wait after a charger is detected to debounce pin contact order */
#define USB_CHG_DEBOUNCE_DELAY_MS 1000
/*
 * Wait after reset, before re-enabling attach interrupt, so that the
 * spurious attach interrupt from certain ports is ignored.
 */
#define USB_CHG_RESET_DELAY_MS 100

/*
 * Store the state of our USB data switches so that they can be restored
 * after pericom reset.
 */
static int usb_switch_state[CONFIG_USB_PD_PORT_COUNT];
static struct mutex usb_switch_lock[CONFIG_USB_PD_PORT_COUNT];

static void update_vbus_supplier(int port, int vbus_level)
{
	struct charge_port_info charge;

	/*
	 * If VBUS is low, or VBUS is high and we are not outputting VBUS
	 * ourselves, then update the VBUS supplier.
	 */
	if (!vbus_level || !usb_charger_port_is_sourcing_vbus(port)) {
		charge.voltage = USB_CHARGER_VOLTAGE_MV;
		charge.current = vbus_level ? USB_CHARGER_MIN_CURR_MA : 0;
		charge_manager_update_charge(CHARGE_SUPPLIER_VBUS,
					     port,
					     &charge);
	}
}

int usb_charger_port_is_sourcing_vbus(int port)
{
	if (port == 0)
		return gpio_get_level(GPIO_USB_C0_5V_EN);
#if CONFIG_USB_PD_PORT_COUNT >= 2
	else if (port == 1)
		return gpio_get_level(GPIO_USB_C1_5V_EN);
#endif
	/* Not a valid port */
	return 0;
}

void usb_charger_set_switches(int port, enum usb_switch setting)
{
	/* If switch is not changing then return */
	if (setting == usb_switch_state[port])
		return;

	mutex_lock(&usb_switch_lock[port]);
	if (setting != USB_SWITCH_RESTORE)
		usb_switch_state[port] = setting;
	usb_chargers[port].driver->set_switches(port, usb_switch_state[port]);
	mutex_unlock(&usb_switch_lock[port]);
}

void usb_charger_vbus_change(int port, int vbus_level)
{
	/* If VBUS has transitioned low, notify PD module directly */
	pd_vbus_low(port);
	/* Update VBUS supplier and signal VBUS change to USB_CHG task */
	update_vbus_supplier(port, vbus_level);
#if CONFIG_USB_PD_PORT_COUNT == 2
	task_set_event(port ? TASK_ID_USB_CHG_P1 : TASK_ID_USB_CHG_P0,
		       USB_CHG_EVENT_VBUS, 0);
#else
	task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_VBUS, 0);
#endif
}

static void usb_charger_bc12_detect(int port)
{
	int device_type, charger_status;
	struct charge_port_info charge;
	int type;

	charge.voltage = USB_CHARGER_VOLTAGE_MV;

	if (usb_charger_port_is_sourcing_vbus(port)) {
		/* If we're sourcing VBUS then we're not charging */
		device_type = charger_status = 0;
	} else {
		/* Set device type */
		device_type = usb_chargers[port].driver->get_dev_type(port);
		charger_status =
			usb_chargers[port].driver->get_chg_status(port);
	}

	/* Debounce pin plug order if we detect a charger */
	if (device_type || usb_chargers[port].driver->get_chg_any_det(
						port, charger_status)) {
		/* next operation might trigger a detach interrupt */
		usb_chargers[port].driver->disable_intr(port);

		/*
		 * Ensure D+/D- are open before resetting
		 * Note: we can't simply call set_switches() because
		 * another task might override it and set the switches closed.
		 */
		usb_chargers[port].driver->set_switch_manual(port, 1);
		usb_chargers[port].driver->set_pins(port, 0);

		/* Delay to debounce pin attach order */
		msleep(USB_CHG_DEBOUNCE_DELAY_MS);

		/*
		 * Trigger chip reset to refresh detection registers.
		 * WARNING: This reset is acceptable for samus_pd,
		 * but may not be acceptable for devices that have
		 * an OTG / device mode, as we may be interrupting
		 * the connection.
		 */
		usb_chargers[port].driver->reset(port);

		/*
		 * Restore data switch settings - switches return to
		 * closed on reset until restored.
		 */
		usb_charger_set_switches(port, USB_SWITCH_RESTORE);
		/* Clear possible disconnect interrupt */
		usb_chargers[port].driver->get_intr(port);
		/* Mask attach interrupt */
		usb_chargers[port].driver->set_intr_mask(port, 1);
		/* Re-enable interrupts */
		usb_chargers[port].driver->enable_intr(port);
		msleep(USB_CHG_RESET_DELAY_MS);

		/* Clear possible attach interrupt */
		usb_chargers[port].driver->get_intr(port);
		/* Re-enable attach interrupt */
		usb_chargers[port].driver->set_intr_mask(port, 0);

		/* Re-read ID registers */
		device_type = usb_chargers[port].driver->get_dev_type(port);
		charger_status =
			usb_chargers[port].driver->get_chg_status(port);
	}

	/* Attachment: decode + update available charge */
	if (device_type || usb_chargers[port].driver->get_chg_any_det(
						port, charger_status)) {
		type = usb_chargers[port].driver->get_chg_type(
						port,
						charger_status,
						device_type);
		charge.current = usb_chargers[port].driver->get_ilim(
						port,
						device_type,
						charger_status);
		charge_manager_update_charge(type, port, &charge);
	} else { /* Detachment: update available charge to 0 */
		charge.current = 0;
		charge_manager_update_charge(
					CHARGE_SUPPLIER_PROPRIETARY,
					port,
					&charge);
		charge_manager_update_charge(
					CHARGE_SUPPLIER_BC12_CDP,
					port,
					&charge);
		charge_manager_update_charge(
					CHARGE_SUPPLIER_BC12_DCP,
					port,
					&charge);
		charge_manager_update_charge(
					CHARGE_SUPPLIER_BC12_SDP,
					port,
					&charge);
		charge_manager_update_charge(
					CHARGE_SUPPLIER_OTHER,
					port,
					&charge);
	}

	/* notify host of power info change */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}

void usb_charger_task(void)
{
	int attach_mask;
	int port = (task_get_current() == TASK_ID_USB_CHG_P0 ? 0 : 1);
	int interrupt;
	uint32_t evt;

	/* Initialize chip and enable interrupts */
	usb_chargers[port].driver->init(port);
	attach_mask = usb_chargers[port].driver->attach_mask(port);

	usb_charger_bc12_detect(port);

	while (1) {
		/* Wait for interrupt */
		evt = task_wait_event(-1);

		/* Interrupt from the Pericom chip, determine charger type */
		if (evt & USB_CHG_EVENT_BC12) {
			/* Read interrupt register to clear on chip */
			usb_chargers[port].driver->get_intr(port);
			usb_charger_bc12_detect(port);
		} else if (evt & USB_CHG_EVENT_INTR) {
			/* Check the interrupt register, and clear on chip */
			interrupt = usb_chargers[port].driver->get_intr(port);
			if (interrupt & attach_mask)
				usb_charger_bc12_detect(port);
		}

		/*
		 * Re-enable interrupts on pericom charger detector since the
		 * chip may periodically reset itself, and come back up with
		 * registers in default state. TODO(crosbug.com/p/33823): Fix
		 * these unwanted resets.
		 */
		if (evt & USB_CHG_EVENT_VBUS) {
			usb_chargers[port].driver->enable_intr(port);
#ifndef CONFIG_USB_PD_TCPM_VBUS
			CPRINTS("VBUS p%d %d", port,
				pd_snk_is_vbus_provided(port));
#endif
		}
	}
}

static void usb_charger_init(void)
{
	int i;
	struct charge_port_info charge_none;

	/* Initialize all pericom charge suppliers to 0 */
	charge_none.voltage = USB_CHARGER_VOLTAGE_MV;
	charge_none.current = 0;
	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++) {
		charge_manager_update_charge(CHARGE_SUPPLIER_PROPRIETARY,
					     i,
					     &charge_none);
		charge_manager_update_charge(CHARGE_SUPPLIER_BC12_CDP,
					     i,
					     &charge_none);
		charge_manager_update_charge(CHARGE_SUPPLIER_BC12_DCP,
					     i,
					     &charge_none);
		charge_manager_update_charge(CHARGE_SUPPLIER_BC12_SDP,
					     i,
					     &charge_none);
		charge_manager_update_charge(CHARGE_SUPPLIER_OTHER,
					     i,
					     &charge_none);

#ifndef CONFIG_USB_PD_TCPM_VBUS
		/* Initialize VBUS supplier based on whether VBUS is present */
		update_vbus_supplier(i, pd_snk_is_vbus_provided(i));
#endif
	}
}
DECLARE_HOOK(HOOK_INIT, usb_charger_init, HOOK_PRIO_DEFAULT);
