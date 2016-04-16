/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * USB charger / BC1.2 task. This is specific to BD99955 charger only.
 */

#include "bd99955.h"
#include "charge_manager.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "hooks.h"
#include "task.h"
#include "timer.h"
#include "usb_charge.h"
#include "usb_pd.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

/* TODO: Add accurate timeout for detecting BC1.2 */
#define BC12_DETECT_RETRY	10

#define USB_CHG_EVENT_DETACH TASK_EVENT_CUSTOM(1)
#define USB_CHG_EVENT_ATTACH TASK_EVENT_CUSTOM(2)

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

static void usb_charger_init(void)
{
	struct charge_port_info charge_none;
	int port;

	/* Initialize all BD99955 charge suppliers to 0 */
	charge_none.voltage = USB_CHARGER_VOLTAGE_MV;
	charge_none.current = 0;

	for (port = 0; port < CONFIG_USB_PD_PORT_COUNT; port++) {
		charge_manager_update_charge(CHARGE_SUPPLIER_PROPRIETARY,
					     port,
					     &charge_none);
		charge_manager_update_charge(CHARGE_SUPPLIER_BC12_CDP,
					     port,
					     &charge_none);
		charge_manager_update_charge(CHARGE_SUPPLIER_BC12_DCP,
					     port,
					     &charge_none);
		charge_manager_update_charge(CHARGE_SUPPLIER_BC12_SDP,
					     port,
					     &charge_none);
		charge_manager_update_charge(CHARGE_SUPPLIER_OTHER,
					     port,
					     &charge_none);

		/* Initialize VBUS supplier based on whether VBUS is present */
		update_vbus_supplier(port, pd_snk_is_vbus_provided(port));
	}
}
DECLARE_HOOK(HOOK_INIT, usb_charger_init, HOOK_PRIO_DEFAULT - 1);

static int usb_charger_bc12_detect(int port)
{
	int i;
	int bc12_type;
	struct charge_port_info charge;

	/*
	 * BC1.2 detection starts 100ms after VBUS/VCC attach and typically
	 * completes 312ms after VBUS/VCC attach.
	 */
	msleep(312);
	for (i = 0; i < BC12_DETECT_RETRY; i++) {
		/* get device type */
		bc12_type = bd99955_get_charger_device_type(port);

		/* Detected BC1.2 */
		if (bc12_type != CHARGE_SUPPLIER_NONE)
			break;

		/* TODO: Add accurate timeout for detecting BC1.2 */
		msleep(100);
	}

	/* BC1.2 device attached */
	if (bc12_type != CHARGE_SUPPLIER_NONE) {
		/* Enable charging trigger by BC1.2 detection */
		bd99955_bc12_enable_charging(port, 1);

		/* Update charge manager */
		charge.voltage = USB_CHARGER_VOLTAGE_MV;
		charge.current = bd99955_get_bc12_ilim(bc12_type);
		charge_manager_update_charge(bc12_type, port, &charge);

		/* notify host of power info change */
		pd_send_host_event(PD_EVENT_POWER_CHANGE);
	}

	return bc12_type;
}

static void usb_charger_detach(int type, int port)
{
	struct charge_port_info charge = {
		.voltage = USB_CHARGER_VOLTAGE_MV,
		.current = 0,
	};

	/* Update charge manager */
	charge_manager_update_charge(type, port, &charge);

	/* notify host of power info change */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}

void usb_charger_vbus_change(int port, int vbus_level)
{
#if CONFIG_USB_PD_PORT_COUNT == 2
	task_set_event(port ? TASK_ID_USB_CHG_P1 : TASK_ID_USB_CHG_P0,
		       vbus_level ? USB_CHG_EVENT_ATTACH : USB_CHG_EVENT_DETACH,
		       0);
#else
	task_set_event(TASK_ID_USB_CHG_P0,
		       vbus_level ? USB_CHG_EVENT_ATTACH : USB_CHG_EVENT_DETACH,
		       0);
#endif
}

void usb_charger_task(void)
{
	int port = (task_get_current() == TASK_ID_USB_CHG_P0 ? 0 : 1);
	uint32_t evt = USB_CHG_EVENT_ATTACH;
	int pd_type = CHARGE_SUPPLIER_NONE;
	int bc12_type = CHARGE_SUPPLIER_NONE;
	int vbus_source;
	int vbus_provided;
	int clear_vbus = 0;

	while (1) {
		vbus_provided = pd_snk_is_vbus_provided(port);

		/* Charger/sync attached */
		if (evt & USB_CHG_EVENT_ATTACH) {
			/* Enable USB switch to detect USB2.0 sync */
			vbus_source = usb_charger_port_is_sourcing_vbus(port);
			if (vbus_source)
				bd99955_enable_usb_switch(port, 1);

			if (vbus_provided) {
				/*
				 * If PD already negotiated then nothing to do.
				 * Otherwise check if there is a BC1.2 charger
				 * attached.
				 */
				pd_type = get_charge_supplier(port);
				if (pd_type != CHARGE_SUPPLIER_PD &&
					pd_type != CHARGE_SUPPLIER_TYPEC) {
					bc12_type =
						usb_charger_bc12_detect(port);
					if (bc12_type != CHARGE_SUPPLIER_NONE &&
						pd_type == CHARGE_SUPPLIER_VBUS)
						clear_vbus = 1;
				}
			}
		}

		/* Charger/sync detached */
		if (evt & USB_CHG_EVENT_DETACH) {
			if (!vbus_provided) {
				/* This is PD detach event */
				if (pd_type != CHARGE_SUPPLIER_NONE &&
					bc12_type == CHARGE_SUPPLIER_NONE) {
					usb_charger_detach(pd_type, port);
					pd_type = CHARGE_SUPPLIER_NONE;
				}

				/* This is BC1.2 detach event */
				if (bc12_type != CHARGE_SUPPLIER_NONE) {
					if (clear_vbus) {
						usb_charger_detach(
							CHARGE_SUPPLIER_VBUS,
							port);
						clear_vbus = 0;
					}

					usb_charger_detach(bc12_type, port);
					bc12_type = CHARGE_SUPPLIER_NONE;

					/*
					 * Disable charging trigger by BC1.2
					 * detection.
					 */
					bd99955_bc12_enable_charging(port, 0);
				}
			}

			/* Disable USB switch to detect BC1.2 source */
			if (vbus_source) {
				bd99955_enable_usb_switch(port, 0);
				vbus_source = 0;
			}
		}

		/* Wait for interrupt */
		evt = task_wait_event(-1);
	}
}
