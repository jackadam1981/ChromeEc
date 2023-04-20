/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Shim driver that connects the ECOS BC1.2 support to an upstream BC1.2
 * driver. This shim can be used with any upstream driver.
 */

#include "usb_charge.h"
#include "usb_pd.h"
#include "usbc/bc12_upstream.h"
#include "usbc/utils.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/usb/usb_bc12.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(shim_bc12, CONFIG_USBC_LOG_LEVEL);

#define BC12_CHIP_ENTRY(usbc_id, bc12_id, chip_fn) \
	[USBC_PORT_NEW(usbc_id)] = chip_fn(bc12_id)

#define CHECK_COMPAT(compat, usbc_id, bc12_id, config)   \
	COND_CODE_1(DT_NODE_HAS_COMPAT(bc12_id, compat), \
		    (BC12_CHIP_ENTRY(usbc_id, bc12_id, config)), ())

#define BC12_SHIM_DRIVER_FIND(usbc_id, bc12_id)                    \
	CHECK_COMPAT(PI3USB9201_UPSTREAM_COMPAT, usbc_id, bc12_id, \
		     DEVICE_DT_GET),

#define BC12_SHIM_DRIVER(usbc_id)                                            \
	COND_CODE_1(                                                         \
		DT_NODE_HAS_PROP(usbc_id, bc12),                             \
		(BC12_SHIM_DRIVER_FIND(usbc_id, DT_PHANDLE(usbc_id, bc12))), \
		({}))

static const struct device *bc12_shim_drivers[] = { DT_FOREACH_STATUS_OKAY(
	named_usbc_port, BC12_SHIM_DRIVER) };

#define BC12_CACHED_ENTRY(unused) BC12_TYPE_NONE,
static enum bc12_type bc12_type_cached[] = { DT_FOREACH_STATUS_OKAY(
	named_usbc_port, BC12_CACHED_ENTRY) };

static enum charge_supplier bc12_type_to_supplier[] = {
	[BC12_TYPE_NONE] = CHARGE_SUPPLIER_NONE,
	[BC12_TYPE_SDP] = CHARGE_SUPPLIER_BC12_SDP,
	[BC12_TYPE_DCP] = CHARGE_SUPPLIER_BC12_DCP,
	[BC12_TYPE_CDP] = CHARGE_SUPPLIER_BC12_CDP,
	[BC12_TYPE_PROPRIETARY] = CHARGE_SUPPLIER_PROPRIETARY,
};

static void bc12_shim_clear_suppliers(int port)
{
	/* Update all suppliers to the NULL supply */
	charge_manager_update_charge(CHARGE_SUPPLIER_BC12_SDP, port, NULL);
	charge_manager_update_charge(CHARGE_SUPPLIER_BC12_DCP, port, NULL);
	charge_manager_update_charge(CHARGE_SUPPLIER_BC12_CDP, port, NULL);
	charge_manager_update_charge(CHARGE_SUPPLIER_PROPRIETARY, port, NULL);
}

const char *bc12_type_name[] = {
	[BC12_TYPE_NONE] = "BC12_TYPE_NONE",
	[BC12_TYPE_SDP] = "BC12_TYPE_SDP",
	[BC12_TYPE_DCP] = "BC12_TYPE_DCP",
	[BC12_TYPE_CDP] = "BC12_TYPE_CDP",
	[BC12_TYPE_PROPRIETARY] = "BC12_TYPE_PROPRIETARY",
	[BC12_TYPE_UNKNOWN] = "BC12_TYPE_UNKNOWN",
};

static void bc12_shim_result_cb(const struct device *dev,
				struct bc12_partner_state *state,
				void *user_data)
{
	int port = (int)user_data;

	if (state) {
		LOG_INF("BC12 callback: type %s", bc12_type_name[state->type]);
	}

	if (state && state->type != BC12_TYPE_NONE) {
		struct charge_port_info charge;

		if (bc12_type_cached[port] != state->type) {
			/* On any changes, clear out the previous supplier info
			 */
			bc12_shim_clear_suppliers(port);
		}

		/*
		 * The Zephyr BC1.2 sets the current limit to 2.5 mA on SDP
		 * ports until the USB bus is not suspended or the USB device
		 * configured. The EC doesn't have any visibility into the USB
		 * configuration state, so set the SDP current to 500 mA to
		 * match the expectations of the charge manager.
		 */
		if (state->type == BC12_TYPE_SDP) {
			charge.current = 500;
		} else {
			/*
			 * Pass through the reported current, converting from
			 * uA to mA.
			 */
			charge.current = state->current_ua / 1000;
		}
		charge.voltage = state->voltage_uv / 1000;

		charge_manager_update_charge(bc12_type_to_supplier[state->type],
					     port, &charge);
	} else {
		bc12_shim_clear_suppliers(port);
	}
}

static void bc12_upstream_usb_charger_task_init(const int port)
{
	const struct device *bc12_dev = bc12_shim_drivers[port];

	if (!bc12_dev)
		return;

	bc12_set_result_cb(bc12_dev, &bc12_shim_result_cb, (void *)port);
}

static void bc12_upstream_usb_charger_task_event(const int port, uint32_t evt)
{
	const struct device *bc12_dev = bc12_shim_drivers[port];

	if (!bc12_dev)
		return;

	if (evt & USB_CHG_EVENT_BC12) {
		LOG_ERR("Shimmed drivers don't support USB_CHG_EVENT_BC12");
		return;
	}

	/*
	 * The legacy BC1.2 drivers support multiple events getting
	 * set and processes in the in order of
	 * USB_CHG_EVENT_VBUS
	 * USB_CHG_EVENT_DR_UFP
	 * USB_CHG_EVENT_DR_DFP
	 * USB_CHG_EVENT_CC_OPEN
	 *
	 * Match that ordering here.
	 */
	if (!IS_ENABLED(CONFIG_USB_PD_VBUS_DETECT_TCPC) &&
	    (evt & USB_CHG_EVENT_VBUS))
		LOG_INF("VBUS p%d %d", port, pd_snk_is_vbus_provided(port));

	if (evt & USB_CHG_EVENT_DR_UFP) {
		bc12_set_role(bc12_dev, BC12_PORTABLE_DEVICE);
	}
	if (evt & USB_CHG_EVENT_DR_DFP) {
		bc12_set_role(bc12_dev, BC12_CHARGING_PORT);
	}
	if (evt & USB_CHG_EVENT_CC_OPEN) {
		bc12_set_role(bc12_dev, BC12_DISCONNECTED);
	}
}

const struct bc12_drv bc12_upstream_drv = {
	.usb_charger_task_init = bc12_upstream_usb_charger_task_init,
	.usb_charger_task_event = bc12_upstream_usb_charger_task_event,
};
