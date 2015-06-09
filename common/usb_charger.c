/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * USB charger / BC1.2 task. This code assumes that CONFIG_CHARGE_MANAGER
 * is defined and implemented. PI3USB9281 is the only charger detector
 * currently supported.
 */

#include "charge_manager.h"
#include "common.h"
#include "ec_commands.h"
#include "gpio.h"
#include "pi3usb9281.h"
#include "task.h"
#include "timer.h"
#include "usb_pd.h"

/* Wait after a charger is detected to debounce pin contact order */
#define USB_CHG_DEBOUNCE_DELAY_MS 1000
/*
 * Wait after reset, before re-enabling attach interrupt, so that the
 * spurious attach interrupt from certain ports is ignored.
 */
#define USB_CHG_RESET_DELAY_MS 100

void usb_charger_task(void)
{
	int port = (task_get_current() == TASK_ID_USB_CHG_P0 ? 0 : 1);
	struct pi3usb9281_config *pericom_chip =
		board_port_to_pi3usb9281_config(port);

	int device_type, charger_status;
	struct charge_port_info charge;
	int type;
	charge.voltage = USB_BC12_CHARGE_VOLTAGE;

	while (1) {
		/* Read interrupt register to clear on chip */
		pi3usb9281_get_interrupts(pericom_chip);

		if (board_is_sourcing_vbus(port)) {
			/* If we're sourcing VBUS then we're not charging */
			device_type = charger_status = 0;
		} else {
			/* Set device type */
			device_type = pi3usb9281_get_device_type(pericom_chip);
			charger_status =
				pi3usb9281_get_charger_status(pericom_chip);
		}

		/* Debounce pin plug order if we detect a charger */
		if (device_type || PI3USB9281_CHG_STATUS_ANY(charger_status)) {
			msleep(USB_CHG_DEBOUNCE_DELAY_MS);

			/*
			 * Trigger chip reset to refresh detection registers.
			 * WARNING: This reset is acceptable for samus_pd,
			 * but may not be acceptable for devices that have
			 * an OTG / device mode, as we may be interrupting
			 * the connection.
			 */
			pi3usb9281_reset(pericom_chip);
			/*
			 * Restore data switch settings - switches return to
			 * closed on reset until restored.
			 */
			board_set_usb_switches(port, USB_SWITCH_RESTORE);
			/* Clear possible disconnect interrupt */
			pi3usb9281_get_interrupts(pericom_chip);
			/* Mask attach interrupt */
			pi3usb9281_set_interrupt_mask(pericom_chip,
						      0xff &
						      ~PI3USB9281_INT_ATTACH);
			/* Re-enable interrupts */
			pi3usb9281_enable_interrupts(pericom_chip);
			msleep(USB_CHG_RESET_DELAY_MS);

			/* Clear possible attach interrupt */
			pi3usb9281_get_interrupts(pericom_chip);
			/* Re-enable attach interrupt */
			pi3usb9281_set_interrupt_mask(pericom_chip, 0xff);

			/* Re-read ID registers */
			device_type = pi3usb9281_get_device_type(pericom_chip);
			charger_status =
				pi3usb9281_get_charger_status(pericom_chip);
		}

		/* Attachment: decode + update available charge */
		if (device_type || PI3USB9281_CHG_STATUS_ANY(charger_status)) {
			if (PI3USB9281_CHG_STATUS_ANY(charger_status))
				type = CHARGE_SUPPLIER_PROPRIETARY;
			else if (device_type & PI3USB9281_TYPE_CDP)
				type = CHARGE_SUPPLIER_BC12_CDP;
			else if (device_type & PI3USB9281_TYPE_DCP)
				type = CHARGE_SUPPLIER_BC12_DCP;
			else if (device_type & PI3USB9281_TYPE_SDP)
				type = CHARGE_SUPPLIER_BC12_SDP;
			else
				type = CHARGE_SUPPLIER_OTHER;

			charge.current = pi3usb9281_get_ilim(device_type,
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

		/* Wait for interrupt */
		task_wait_event(-1);
	}
}
