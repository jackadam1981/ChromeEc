/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Richtek RT1739 USB-C Power Path Controller */
#include "atomic.h"
#include "common.h"
#include "config.h"
#include "console.h"
#include "driver/ppc/rt1739.h"
#include "driver/tcpm/tcpci.h"
#include "hooks.h"
#include "usbc_ppc.h"
#include "util.h"


#if defined(CONFIG_USBC_PPC_VCONN) && !defined(CONFIG_USBC_PPC_POLARITY)
#error "Can't use set_vconn without set_polarity"
#endif

#define RT1739_FLAGS_SOURCE_ENABLED BIT(0)
static atomic_t flags[CONFIG_USB_PD_PORT_MAX_COUNT];

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)

static int read_reg(uint8_t port, int reg, int *val)
{
	return i2c_read8(
		ppc_chips[port].i2c_port,
		ppc_chips[port].i2c_addr_flags,
		reg, val);
}

static int write_reg(uint8_t port, int reg, int val)
{
	return i2c_write8(
		ppc_chips[port].i2c_port,
		ppc_chips[port].i2c_addr_flags,
		reg, val);
}

static int update_reg(int port, int reg, int mask,
		      enum mask_update_action action)
{
	return i2c_update8(
		ppc_chips[port].i2c_port,
		ppc_chips[port].i2c_addr_flags,
		reg, mask, action);
}

static int rt1739_is_sourcing_vbus(int port)
{
	return (flags[port] & RT1739_FLAGS_SOURCE_ENABLED);
}

static int rt1739_vbus_source_enable(int port, int enable)
{
	atomic_t prev_flag;

	if (enable)
		prev_flag = atomic_or(&flags[port],
				RT1739_FLAGS_SOURCE_ENABLED);
	else
		prev_flag = atomic_clear_bits(&flags[port],
				RT1739_FLAGS_SOURCE_ENABLED);

	/* Return if status doesn't change */
	if (!!(prev_flag & RT1739_FLAGS_SOURCE_ENABLED) == !!enable)
		return EC_SUCCESS;

	RETURN_ERROR(update_reg(port, RT1739_REG_VBUS_SWITCH_CTRL,
			        RT1739_REG_VBUS_SWITCH_CTRL_LV_SRC_EN,
				enable ? MASK_SET : MASK_CLR));

#if defined(CONFIG_USB_CHARGER) && defined(CONFIG_USB_PD_VBUS_DETECT_PPC)
	/*
	 * Since the VBUS state could be changing here, need to wake the
	 * USB_CHG_N task so that BC 1.2 detection will be triggered.
	 */
	usb_charger_vbus_change(port, enable);
#endif

	return EC_SUCCESS;
}

static int rt1739_vbus_sink_enable(int port, int enable)
{
	return update_reg(port, RT1739_REG_VBUS_SWITCH_CTRL,
			  RT1739_REG_VBUS_SWITCH_CTRL_HV_SNK_EN,
			  enable ? MASK_SET : MASK_CLR);

}

static int rt1739_discharge_vbus(int port, int enable)
{
	/* TODO: not implemented */

	return EC_SUCCESS;
}

#ifdef CONFIG_CMD_PPC_DUMP
static int rt1739_dump(int port)
{
	for (int i = 0; i <= 0x61; i++) {
		int val = 0;
		int rt = read_reg(port, i, &val);

		if (i % 16 == 0)
			CPRINTF("%02X: ", i);
		if (rt)
			CPRINTF("-- ");
		else
			CPRINTF("%02X ", val);
		if (i % 16 == 15)
			CPRINTF("\n");
	}

	return EC_SUCCESS;
}
#endif /* defined(CONFIG_CMD_PPC_DUMP) */

#ifdef CONFIG_USB_PD_VBUS_DETECT_PPC
static int rt1739_is_vbus_present(int port)
{
	__maybe_unused static atomic_t vbus_prev[CONFIG_USB_PD_PORT_MAX_COUNT];
	int status, vbus;

	if (read_reg(port, RT1739_REG_INT_STS4, &status))
		return 0;

	vbus = !!(status & RT1739_REG_INT_STS4_VBUS_VALID);

#ifdef CONFIG_USB_CHARGER
	if (!!(vbus_prev[port] != vbus))
		usb_charger_vbus_change(port, vbus);

	if (vbus)
		atomic_or(&vbus_prev[port], 1);
	else
		atomic_clear(&vbus_prev[port]);
#endif

	return vbus;
}
#endif

#ifdef CONFIG_USBC_PPC_POLARITY
static int rt1739_set_polarity(int port, int polarity)
{
	return update_reg(port, RT1739_REG_VCONN_CTRL1,
			  RT1739_REG_VCONN_CTRL1_VCONN_ORIENT,
			  polarity ? MASK_SET : MASK_CLR);
}
#endif

#ifdef CONFIG_USBC_PPC_VCONN
static int rt1739_set_vconn(int port, int enable)
{
	RETURN_ERROR(update_reg(port, 0x60, 0x08, enable ? MASK_SET : MASK_CLR));

	RETURN_ERROR(update_reg(port, RT1739_REG_VCONN_CTRL1,
				RT1739_REG_VCONN_CTRL1_VCONN_EN,
				enable ? MASK_SET : MASK_CLR));
	return EC_SUCCESS;
}
#endif

static int rt1739_init(int port)
{
	atomic_clear(&flags[port]);

	RETURN_ERROR(write_reg(port, RT1739_REG_SW_RESET, 1));
	usleep(1 * MSEC);
	RETURN_ERROR(write_reg(port, RT1739_REG_SYS_CTRL,
			       RT1739_REG_SYS_CTRL_OT_EN |
			       RT1739_REG_SYS_CTRL_SHUTDOWN_OFF));
	RETURN_ERROR(write_reg(port, 0x26, 0x66)); /* ES1 workaround */
	RETURN_ERROR(update_reg(port, RT1739_REG_INT_MASK5,
				RT1739_REG_INT_MASK5_BC12_SNK_DONE, MASK_SET));
	RETURN_ERROR(update_reg(port, RT1739_REG_VBUS_DET_EN,
				RT1739_REG_VBUS_DET_EN_VBUS_PRESENT, MASK_SET));
	RETURN_ERROR(update_reg(port, RT1739_REG_SBU_CTRL_01,
				RT1739_REG_SBU_CTRL_DM_SWEN |
				RT1739_REG_SBU_CTRL_DP_SWEN,
				MASK_SET));
	/* TODO: need cleanup */
	RETURN_ERROR(write_reg(port, 0x24, 0b01100110)); /* VBUS OVP -> 23V */
	RETURN_ERROR(write_reg(port, 0x25, 0b00110010)); /* VBUS OCP -> 3.3A */

	return EC_SUCCESS;
}

static int rt1739_get_bc12_ilim(int charge_supplier)
{
	switch (charge_supplier) {
	case CHARGE_SUPPLIER_BC12_DCP:
	case CHARGE_SUPPLIER_BC12_CDP:
		return USB_CHARGER_MAX_CURR_MA;
	case CHARGE_SUPPLIER_BC12_SDP:
	default:
		return USB_CHARGER_MIN_CURR_MA;
	}
}

static void rt1739_update_charge_manager(int port,
					 enum charge_supplier new_bc12_type)
{
	static enum charge_supplier current_bc12_type = CHARGE_SUPPLIER_NONE;

	if (new_bc12_type != current_bc12_type) {
		if (current_bc12_type >= 0)
			charge_manager_update_charge(current_bc12_type, port,
							NULL);

		if (new_bc12_type != CHARGE_SUPPLIER_NONE) {
			struct charge_port_info chg = {
				.current = rt1739_get_bc12_ilim(new_bc12_type),
				.voltage = USB_CHARGER_VOLTAGE_MV,
			};

			charge_manager_update_charge(new_bc12_type, port, &chg);
		}

		current_bc12_type = new_bc12_type;
	}
}

static void rt1739_enable_bc12_detection(int port, bool enable)
{
	update_reg(port, RT1739_REG_BC12_SNK_FUNC,
		   RT1739_REG_BC12_SNK_FUNC_BC12_SNK_EN,
		   enable ? MASK_SET : MASK_CLR);
}

static enum charge_supplier rt1739_bc12_get_device_type(int port)
{
	int reg, bc12_type;

	if (read_reg(port, RT1739_REG_BC12_STAT, &reg))
		return CHARGE_SUPPLIER_NONE;

	bc12_type = reg & RT1739_REG_BC12_STAT_PORT_STAT_MASK;
	switch (bc12_type) {
	case RT1739_REG_BC12_STAT_SDP:
		CPRINTS("BC12 SDP");
		return CHARGE_SUPPLIER_BC12_SDP;
	case RT1739_REG_BC12_STAT_CDP:
		CPRINTS("BC12 CDP");
		return CHARGE_SUPPLIER_BC12_CDP;
	case RT1739_REG_BC12_STAT_DCP:
		CPRINTS("BC12 DCP");
		return CHARGE_SUPPLIER_BC12_DCP;
	default:
		CPRINTS("BC12 UNKNOWN 0x%02X", bc12_type);
		return CHARGE_SUPPLIER_NONE;
	}
}

static void rt1739_usb_charger_task(const int port)
{
	rt1739_enable_bc12_detection(port, false);

	while (1) {
		uint32_t evt = task_wait_event(-1);
		bool is_non_pd_sink = !pd_capable(port) &&
			pd_get_power_role(port) == PD_ROLE_SINK &&
			pd_snk_is_vbus_provided(port);

		/* vbus change, start bc12 detection */
		if (evt & USB_CHG_EVENT_VBUS) {
			if (is_non_pd_sink)
				rt1739_enable_bc12_detection(port, true);
			else
				rt1739_update_charge_manager(
						port, CHARGE_SUPPLIER_NONE);
		}

		/* detection done, update charge_manager and stop detection */
		if (evt & USB_CHG_EVENT_BC12) {
			enum charge_supplier supplier;

			if (is_non_pd_sink)
				supplier = rt1739_bc12_get_device_type(port);
			else
				supplier = CHARGE_SUPPLIER_NONE;
			rt1739_update_charge_manager(port, supplier);
			rt1739_enable_bc12_detection(port, false);
		}
	}
}

static atomic_t pending_events;

void rt1739_deferred_interrupt(void)
{
	atomic_t current = atomic_clear(&pending_events);

	for (int port = 0; port < CONFIG_USB_PD_PORT_MAX_COUNT; ++port) {
		int reg;

		if (!(current & BIT(port)))
			continue;

		if (read_reg(port, RT1739_REG_INT_EVENT5, &reg))
			continue;

		if (reg & RT1739_REG_INT_EVENT5_BC12_SNK_DONE)
			task_set_event(USB_CHG_PORT_TO_TASK_ID(port),
				       USB_CHG_EVENT_BC12);

		write_reg(port, RT1739_REG_INT_EVENT5, reg);
	}
}
DECLARE_DEFERRED(rt1739_deferred_interrupt);

void rt1739_interrupt(int port) {
	atomic_or(&pending_events, BIT(port));
	hook_call_deferred(&rt1739_deferred_interrupt_data, 0);
}

const struct ppc_drv rt1739_ppc_drv = {
	.init = &rt1739_init,
	.is_sourcing_vbus = &rt1739_is_sourcing_vbus,
	.vbus_sink_enable = &rt1739_vbus_sink_enable,
	.vbus_source_enable = &rt1739_vbus_source_enable,
#ifdef CONFIG_CMD_PPC_DUMP
	.reg_dump = &rt1739_dump,
#endif /* defined(CONFIG_CMD_PPC_DUMP) */

#ifdef CONFIG_USB_PD_VBUS_DETECT_PPC
	.is_vbus_present = &rt1739_is_vbus_present,
#endif /* defined(CONFIG_USB_PD_VBUS_DETECT_PPC) */
	.discharge_vbus = &rt1739_discharge_vbus,
#ifdef CONFIG_USBC_PPC_POLARITY
	.set_polarity = &rt1739_set_polarity,
#endif
#ifdef CONFIG_USBC_PPC_VCONN
	.set_vconn = &rt1739_set_vconn,
#endif
#ifdef CONFIG_USB_PD_FRS_PPC
	/* TODO: not implemented */
	/* .set_frs_enable = rt1739_set_frs_enable, */
#endif
};

const struct bc12_drv rt1739_bc12_drv = {
	.usb_charger_task = rt1739_usb_charger_task,
};

#ifdef CONFIG_BC12_SINGLE_DRIVER
/* provide a default bc12_ports[] for backward compatibility */
struct bc12_config bc12_ports[CHARGE_PORT_COUNT] = {
	[0 ... (CHARGE_PORT_COUNT - 1)] = {
		.drv = &rt1739_bc12_drv,
	},
};
#endif /* CONFIG_BC12_SINGLE_DRIVER */
