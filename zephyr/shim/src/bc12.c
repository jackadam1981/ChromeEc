/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "usb_charge.h"
#include "usb_pd.h"
#include "usbc/bc12_pi3usb9201.h"
#include "usbc/bc12_rt1718s.h"
#include "usbc/bc12_rt1739.h"
#include "usbc/bc12_rt9490.h"
#include "usbc/tcpc_rt1718s.h"
#include "usbc/utils.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/usb/usb_bc12.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(shim_bc12, CONFIG_USBC_LOG_LEVEL);

/* Check RT1718S dependency. BC12 node must be dependent on TCPC node. */
#if DT_HAS_COMPAT_STATUS_OKAY(RT1718S_BC12_COMPAT)
BUILD_ASSERT(DT_HAS_COMPAT_STATUS_OKAY(RT1718S_TCPC_COMPAT));
#endif

/* Shim driver that connects to upstream BC1.2 drivers */
#define PI3USB9201_UPSTREAM_COMPAT pericom_pi3usb9201

static void bc12_shim_usb_charger_task_init(const int port);
static void bc12_shim_usb_charger_task_event(const int port, uint32_t evt);

const struct bc12_drv bc12_shim_drv = {
	.usb_charger_task_init = bc12_shim_usb_charger_task_init,
	.usb_charger_task_event = bc12_shim_usb_charger_task_event,
};

#define BC12_CHIP_SHIM(id)             \
	{                              \
		.drv = &bc12_shim_drv, \
	},

#define BC12_CHIP_ENTRY(usbc_id, bc12_id, chip_fn) \
	[USBC_PORT_NEW(usbc_id)] = chip_fn(bc12_id)

#define CHECK_COMPAT(compat, usbc_id, bc12_id, config)   \
	COND_CODE_1(DT_NODE_HAS_COMPAT(bc12_id, compat), \
		    (BC12_CHIP_ENTRY(usbc_id, bc12_id, config)), ())

#define BC12_CHIP_FIND(usbc_id, bc12_id)                                       \
	CHECK_COMPAT(RT1718S_BC12_COMPAT, usbc_id, bc12_id, BC12_CHIP_RT1718S) \
	CHECK_COMPAT(RT1739_BC12_COMPAT, usbc_id, bc12_id, BC12_CHIP_RT1739)   \
	CHECK_COMPAT(RT9490_BC12_COMPAT, usbc_id, bc12_id, BC12_CHIP_RT9490)   \
	CHECK_COMPAT(PI3USB9201_COMPAT, usbc_id, bc12_id,                      \
		     BC12_CHIP_PI3USB9201)                                     \
	CHECK_COMPAT(PI3USB9201_UPSTREAM_COMPAT, usbc_id, bc12_id,             \
		     BC12_CHIP_SHIM)

#define BC12_CHIP(usbc_id)                           \
	COND_CODE_1(DT_NODE_HAS_PROP(usbc_id, bc12), \
		    (BC12_CHIP_FIND(usbc_id, DT_PHANDLE(usbc_id, bc12))), ())

/* BC1.2 controllers */
struct bc12_config bc12_ports[CHARGE_PORT_COUNT] = { DT_FOREACH_STATUS_OKAY(
	named_usbc_port, BC12_CHIP) };

#ifdef CONFIG_USB_BC12

#define BC12_SHIM_DRIVER_FIND(usbc_id, bc12_id)                    \
	CHECK_COMPAT(PI3USB9201_UPSTREAM_COMPAT, usbc_id, bc12_id, \
		     DEVICE_DT_GET),

#define BC12_SHIM_DRIVER(usbc_id)                                            \
	COND_CODE_1(                                                         \
		DT_NODE_HAS_PROP(usbc_id, bc12),                             \
		(BC12_SHIM_DRIVER_FIND(usbc_id, DT_PHANDLE(usbc_id, bc12))), \
		())

static const struct device *bc12_shim_drivers[] = { DT_FOREACH_STATUS_OKAY(
	named_usbc_port, BC12_SHIM_DRIVER) };

static enum charge_supplier bc12_type_to_supplier[] = {
	[BC12_TYPE_NONE] = CHARGE_SUPPLIER_NONE,
	[BC12_TYPE_SDP] = CHARGE_SUPPLIER_BC12_SDP,
	[BC12_TYPE_DCP] = CHARGE_SUPPLIER_BC12_DCP,
	[BC12_TYPE_CDP] = CHARGE_SUPPLIER_BC12_CDP,
	[BC12_TYPE_PROPRIETARY] = CHARGE_SUPPLIER_PROPRIETARY,
};

static void bc12_shim_result_cb(const struct device *dev,
				struct bc12_partner_state *state,
				void *user_data)
{
	int port = (int)user_data;
	if (state && state->type != BC12_TYPE_NONE) {
		struct charge_port_info charge;

		charge.current = state->current;
		charge.voltage = state->voltage;
		charge_manager_update_charge(bc12_type_to_supplier[state->type],
					     port, &charge);
	} else {
		/* Update all suppliers to the NULL supply */
		charge_manager_update_charge(CHARGE_SUPPLIER_BC12_SDP, port,
					     NULL);
		charge_manager_update_charge(CHARGE_SUPPLIER_BC12_DCP, port,
					     NULL);
		charge_manager_update_charge(CHARGE_SUPPLIER_BC12_CDP, port,
					     NULL);
		charge_manager_update_charge(CHARGE_SUPPLIER_PROPRIETARY, port,
					     NULL);
	}
}

static void bc12_shim_usb_charger_task_init(const int port)
{
	const struct device *bc12_dev = bc12_shim_drivers[port];

	if (!bc12_dev)
		return;

	bc12_result_cb(bc12_dev, &bc12_shim_result_cb, (void *)port);
}

static void bc12_shim_usb_charger_task_event(const int port, uint32_t evt)
{
	const struct device *bc12_dev = bc12_shim_drivers[port];

	if (!bc12_dev)
		return;

	if (evt & USB_CHG_EVENT_BC12) {
		bc12_set_role(bc12_dev, BC12_INTERRUPT);
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

#endif /* CONFIG_USB_BC12 */
