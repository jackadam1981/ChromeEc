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

static enum usb_switch usb_switch_state[CONFIG_USB_PD_PORT_COUNT];
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

		usb_switch_state[port] = USB_SWITCH_RESTORE;
	}
}
DECLARE_HOOK(HOOK_INIT, usb_charger_init, HOOK_PRIO_INIT_USB_CHARGER);

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

static void usb_charger_detach(int port, int type)
{
	struct charge_port_info charge = {
		.voltage = USB_CHARGER_VOLTAGE_MV,
		.current = 0,
	};

	/* Update charge manager */
	charge_manager_update_charge(CHARGE_SUPPLIER_VBUS, port, &charge);
	charge_manager_update_charge(type, port, &charge);

	/* Disable charging trigger by BC1.2 detection */
	bd99955_bc12_enable_charging(port, 0);

	/* notify host of power info change */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}

#ifdef USB_CHARGER_VBUS_INTERRUPT
static void bd99955_vbus_interrupt_def(void)
{
	int port;
	int intr;
	task_id_t usb_chg_tskid[CONFIG_USB_PD_PORT_COUNT] = {
		TASK_ID_USB_CHG_P0,
#if CONFIG_USB_PD_PORT_COUNT == 2
		TASK_ID_USB_CHG_P1,
#endif
	};
	task_id_t pd_chg_tskid[CONFIG_USB_PD_PORT_COUNT] = {
		TASK_ID_PD_C0,
#if CONFIG_USB_PD_PORT_COUNT == 2
		TASK_ID_PD_C1,
#endif
	};

	for (port = 0; port < CONFIG_USB_PD_PORT_COUNT; port++) {
		/* Get the VBUS interrupt */
		intr = bd99955_get_vbus_detect_interrupts(port, 1);
		if (!intr)
			continue;

		/* VBUS is detected */
		if (intr & BD99955_CMD_INT_SET_DET) {
			task_set_event(usb_chg_tskid[port],
				USB_CHG_EVENT_ATTACH,
				0);
		}

		/* VBUS is reset */
		if (intr & BD99955_CMD_INT_SET_RES) {
			task_set_event(usb_chg_tskid[port],
				USB_CHG_EVENT_DETACH,
				0);
		}

		task_wake(pd_chg_tskid[port]);

		/* Clear the VBUS interrupt */
		bd99955_get_vbus_detect_interrupts(port, 0);
	}
}
DECLARE_DEFERRED(bd99955_vbus_interrupt_def);
#endif  /* USB_CHARGER_VBUS_INTERRUPT */

void usb_charger_set_switches(int port, enum usb_switch setting)
{
	/* If switch is not changing then return */
	if (setting == usb_switch_state[port])
		return;

	mutex_lock(&usb_switch_lock[port]);
	if (setting != USB_SWITCH_RESTORE)
		usb_switch_state[port] = setting;
	bd99955_enable_usb_switch(port, usb_switch_state[port]);
	mutex_unlock(&usb_switch_lock[port]);
}

#ifdef USB_CHARGER_VBUS_INTERRUPT
void usb_charger_vbus_interrupt(enum gpio_signal signal)
{
	hook_call_deferred(&bd99955_vbus_interrupt_def_data, 0);
}
#else
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
#endif /* USB_CHARGER_VBUS_INTERRUPT */

void usb_charger_task(void)
{
	int port = (task_get_current() == TASK_ID_USB_CHG_P0 ? 0 : 1);
	uint32_t evt = USB_CHG_EVENT_ATTACH;
	int bc12_type = CHARGE_SUPPLIER_NONE;

	while (1) {
		/* Charger/sync attached */
		if (evt & USB_CHG_EVENT_ATTACH) {
			if (pd_snk_is_vbus_provided(port))
				bc12_type = usb_charger_bc12_detect(port);
		}

		/* Charger/sync detached */
		if (evt & USB_CHG_EVENT_DETACH) {
			if (bc12_type != CHARGE_SUPPLIER_NONE &&
				!pd_snk_is_vbus_provided(port)) {
				usb_charger_detach(port, bc12_type);
				bc12_type = CHARGE_SUPPLIER_NONE;
			}
		}

		/* Wait for interrupt */
		evt = task_wait_event(-1);
	}
}
