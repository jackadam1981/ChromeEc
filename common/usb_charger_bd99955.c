/* Copyright 2015 The Chromium OS Authors. All rights reserved.
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
#include "gpio.h"
#include "hooks.h"
#include "task.h"
#include "timer.h"
#include "usb_charge.h"
#include "usb_pd.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

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

static void usb_charger_init(int port)
{
	struct charge_port_info charge_none;

	/* Initialize all pericom charge suppliers to 0 */
	charge_none.voltage = USB_CHARGER_VOLTAGE_MV;
	charge_none.current = 0;

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

int usb_charger_bc12_detect(int port)
{
	int i;
	int type;

	/* BC1.2 detection may take up to 1s hence retry */
	for (i = 0; i < BC12_DETECT_RETRY; i++) {
		/* get device type */
		type = bd99955_get_charger_device_type(port);

		/* Detected BC1.2 */
		if (type != CHARGE_SUPPLIER_NONE)
			break;

		msleep(100);
	}

	return type;
}

void usb_charger_task(void)
{
	int port = (task_get_current() == TASK_ID_USB_CHG_P0 ? 0 : 1);
	uint32_t evt;
	struct charge_port_info charge;
	int type;

	usb_charger_init(port);

	while (1) {
		/* Wait for interrupt */
		evt = task_wait_event(-1);

		/* Ignore the debounce */
		if (evt == (USB_CHG_EVENT_DETACH | USB_CHG_EVENT_ATTACH))
			continue;

		type = usb_charger_bc12_detect(port);

		/* Charger detached */
		if (evt & USB_CHG_EVENT_DETACH) {
			/* Disable charging trigger by BC1.2 detection */
			bd99955_bc12_enable_charging(port, 0);

			/* Update charger manager to default values */
			if (type != CHARGE_SUPPLIER_NONE)
				usb_charger_init(port);
			else
				update_vbus_supplier(port, 0);
		}

		/* Charger attached */
		if (evt & USB_CHG_EVENT_ATTACH) {
			/* BC1.2 device attached */
			if (type != CHARGE_SUPPLIER_NONE) {
				/* Enable charging trigger by BC1.2 detection */
				bd99955_bc12_enable_charging(port, 1);

				charge.voltage = USB_CHARGER_VOLTAGE_MV;
				charge.current = bd99955_get_bc12_ilim(type);
				charge_manager_update_charge(type,
							port,
							&charge);
			} else {
				/*
				 * Disable charging trigger by BC1.2
				 * detection
				 */
				bd99955_bc12_enable_charging(port, 0);

				update_vbus_supplier(port, 1);
			}
		}

		/* notify host of power info change */
		pd_send_host_event(PD_EVENT_POWER_CHANGE);
	}
}
