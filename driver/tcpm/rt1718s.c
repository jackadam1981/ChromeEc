/* Copyright 2021 The Richtek Technology Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * RT1718S TCPC Driver
 */

#include "console.h"
#include "driver/tcpm/rt1718s.h"
#include "driver/tcpm/tcpci.h"
#include "driver/tcpm/tcpm.h"
#include "stdint.h"
#include "task.h"
#include "timer.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

#define ENABLE_FAST_ROLE_SWAP		0

/* i2c_write function which won't wake TCPC from low power mode. */
int rt1718s_write8(int port, int reg, int val)
{
	if (reg > 0xFF) {
		return i2c_write_offset16(
			tcpc_config[port].i2c_info.port,
			tcpc_config[port].i2c_info.addr_flags,
			reg, val, 1);
	} else {
		return i2c_write8(
			tcpc_config[port].i2c_info.port,
			tcpc_config[port].i2c_info.addr_flags,
			reg, val);
	}
}

int rt1718s_read8(int port, int reg, int *val)
{
	if (reg > 0xFF) {
		return i2c_read_offset16(
			tcpc_config[port].i2c_info.port,
			tcpc_config[port].i2c_info.addr_flags,
			reg, val, 1);
	} else {
		return i2c_read8(
			tcpc_config[port].i2c_info.port,
			tcpc_config[port].i2c_info.addr_flags,
			reg, val);
	}
}

int rt1718s_update_bits8(int port, int reg, int mask, int val)
{
	int reg_val;

	if (mask == 0xFF)
		return rt1718s_write8(port, reg, val);

	RETURN_ERROR(rt1718s_read8(port, reg, &reg_val));

	reg_val &= (~mask);
	reg_val |= (mask & val);
	return rt1718s_write8(port, reg, reg_val);
}

static int rt1718s_sw_reset(int port)
{
	int rv;

	rv = rt1718s_update_bits8(port, RT1718S_SYS_CTRL3,
		RT1718S_SWRESET_MASK,RT1718S_SWRESET_MASK);

	msleep(2);

	return rv;
}

/* enable bc 1.2 source function  */
static int rt1718s_enable_bc12_source(int port, bool en)
{
	return rt1718s_update_bits8(port,RT1718S_RT2_BC12_SRC_FUNC, RT1718S_RT2_BC12_SRC_FUNC_BC12_SRC_EN,
								en ? RT1718S_RT2_BC12_SRC_FUNC_BC12_SRC_EN : 0);
}

/* enable bc 1.2 sink function  */
static int rt1718s_enable_bc12_sink(int port, bool en)
{
	return rt1718s_update_bits8(port, RT1718S_RT2_BC12_SNK_FUNC, RT1718S_RT2_BC12_SNK_FUNC_BC12_SNK_EN,
                                en ? RT1718S_RT2_BC12_SNK_FUNC_BC12_SNK_EN : 0);
}


static int rt1718s_set_bc12_source_mode(int port, uint8_t src_mode)
{
	return rt1718s_update_bits8(port,
			RT1718S_RT2_BC12_SRC_FUNC,
			RT1718S_RT2_BC12_SRC_FUNC_SRC_MODE_SEL_MASK, src_mode);
}

static int rt1718s_set_bc12_src_wait_vbus_on(int port, bool en)
{
	return rt1718s_update_bits8(port,
			RT1718S_RT2_BC12_SRC_FUNC,
			RT1718S_RT2_BC12_SRC_FUNC_WAIT_VBUS_ON, en ? 0xFF : 0);
}

static int rt1718s_set_bc12_sink_spec_ta(int port,bool en)
{
	return rt1718s_update_bits8(port,
			RT1718S_RT2_BC12_SNK_FUNC,
			RT1718S_RT2_BC12_SNK_FUNC_SPEC_TA_EN, en ? 0xFF : 0);
}

static int rt1718s_set_bc12_sink_dcdt_sel(int port, uint8_t dcdt_sel)
{
	return rt1718s_update_bits8(port,
			RT1718S_RT2_BC12_SNK_FUNC,
			RT1718S_RT2_BC12_SNK_FUNC_DCDT_SEL_MASK, dcdt_sel);
}

static int rt1718s_set_bc12_sink_vlgc_option(int port, bool en)
{
	return rt1718s_update_bits8(port,
			RT1718S_RT2_BC12_SNK_FUNC,
			RT1718S_RT2_BC12_SNK_FUNC_VLGC_OPT, en ? 0xFF : 0);
}

static int rt1718s_set_bc12_sink_vport_sel(int port, uint8_t sel)
{
	return rt1718s_update_bits8(port,
			RT1718S_RT2_DPDM_CTR1_DPDM_SET,
			RT1718S_RT2_DPDM_CTR1_DPDM_SET_DPDM_VSRC_SEL_MASK, sel);
}

static int rt1718s_set_bc12_sink_wait_vbus(int port, bool en)
{
	return rt1718s_update_bits8(port,
			RT1718S_RT2_BC12_SNK_FUNC,
			RT1718S_RT2_BC12_SNK_FUNC_BC12_WAIT_VBUS, en ? 0xFF : 0);
}

/*
 * rt1718s BC12 function initial
 */
static int rt1718s_bc12_init(int port)
{
	int rv;

	/* enable vendor defined BC12 function */
	rv = rt1718s_write8(port, RT1718S_RT_MASK6, 
							 (RT1718S_RT_MASK6_M_BC12_SNK_DONE |
							 RT1718S_RT_MASK6_M_BC12_TA_CHG));
	if (rv)
		return rv;

	/* set vendor defince alert unmasked */
	rv = rt1718s_update_bits8(port, 0x13, 0x80, 0x80);
	if (rv)
		return rv;

	/* RT2 0x3A = 0x43 */
	rv = rt1718s_write8(port,RT1718S_RT2_SBU_CTRL_01,
						(RT1718S_RT2_SBU_CTRL_01_DPDM_VIEN |
						RT1718S_RT2_SBU_CTRL_01_DM_SWEN |
						RT1718S_RT2_SBU_CTRL_01_DP_SWEN));
	if (rv)
		return rv;

	/* enable BC12 source mode */
	rv = rt1718s_set_bc12_source_mode(port, RT1718S_RT2_BC12_SRC_FUNC_SRC_MODE_SEL_BC12_SDP);
	if (rv)
		return rv;

	/* enable source wait vbus on */
	rv = rt1718s_set_bc12_src_wait_vbus_on(port, false);
	if (rv)
		return rv;

	/* disable bv 1.2 source function */
	rv = rt1718s_enable_bc12_source(port, false);
	if (rv)
		return rv;

	/* disable 2.7v mode */
	rv = rt1718s_set_bc12_sink_spec_ta(port, false);
	if (rv)
		return rv;

	/* dcdt select 600ms timeout */ 
	rv = rt1718s_set_bc12_sink_dcdt_sel(port, RT1718S_RT2_BC12_SNK_FUNC_DCDT_SEL_600MS);
	if (rv)
		return rv;

	/* disable vlgc option */
	rv = rt1718s_set_bc12_sink_vlgc_option(port, false);
	if (rv)
		return rv;

	/* DPDM voltage selection */
	rv = rt1718s_set_bc12_sink_vport_sel(port, RT1718S_RT2_DPDM_CTR1_DPDM_SET_DPDM_VSRC_SEL_0_65V);
	if (rv)
		return rv;

	/* disable sink wait vbus */
	rv = rt1718s_set_bc12_sink_wait_vbus(port, false);
	if (rv)
		return rv;

	/* enable bv 1.2 sink function */
	rv = rt1718s_enable_bc12_sink(port, false);

	return rv;
}

static int rt1718s_init(int port)
{
	int sys_ctrl1;

	RETURN_ERROR(rt1718s_read8(port, RT1718S_SYS_CTRL1, &sys_ctrl1));

	if (!(sys_ctrl1 & 0x20))
		RETURN_ERROR(rt1718s_sw_reset(port));

#if ENABLE_FAST_ROLE_SWAP
	/* set vbus frs low unmasked, Rx frs unmasked */
	RETURN_ERROR(rt1718s_update_bits8(port, 0x91, 0xC0, 0xC0));
#endif
	/* set vendor defined alert unmasked */
	RETURN_ERROR(rt1718s_update_bits8(port, 0x13, 0x80, 0x80));

	/* set vbus frs low unmasked, Rx frs unmasked */
	RETURN_ERROR(rt1718s_update_bits8(port, 0x91, 0xC0, 0xC0));

	RETURN_ERROR(rt1718s_bc12_init(port));

	/* Disable FOD function */
	RETURN_ERROR(rt1718s_update_bits8(port, 0xCF, 0x40, 0x00));

	/* tcpc connect invalid disabled. Exit shipping mode */
	RETURN_ERROR(rt1718s_update_bits8(port, RT1718S_SYS_CTRL1, 0x60, 0x20));

	RETURN_ERROR(rt1718s_write8(port, 0x1F, 0xFF));

	RETURN_ERROR(rt1718s_write8(port, 0x11, 0xFF));

	RETURN_ERROR(rt1718s_update_bits8(port, 0x8C, 0x02, 0x02));

	RETURN_ERROR(rt1718s_write8(port, 0xF23A, 0x43));

	RETURN_ERROR(tcpci_tcpm_init(port));

	RETURN_ERROR(rt1718s_update_bits8(port, 0x13, 0x80, 0x80));

	RETURN_ERROR(rt1718s_write8(port, 0xF211, 0x20));

	RETURN_ERROR(board_rt1718s_init(port));

	return 0;
}

__overridable int board_rt1718s_init(int port)
{
	return EC_SUCCESS;
}

static enum charge_supplier rt1718s_get_bc12_type(int port)
{
	int data;

	if(rt1718s_read8(port, RT1718S_RT2_BC12_STAT, &data))
		return CHARGE_SUPPLIER_OTHER;

	switch (data & RT1718S_RT2_BC12_STAT_PORT_STATUS_MASK) {
		case RT1718S_RT2_BC12_STAT_PORT_STATUS_NONE:
			return CHARGE_SUPPLIER_NONE;
		case RT1718S_RT2_BC12_STAT_PORT_STATUS_SDP:
			return CHARGE_SUPPLIER_BC12_SDP;
		case RT1718S_RT2_BC12_STAT_PORT_STATUS_CDP:
			return CHARGE_SUPPLIER_BC12_CDP;
		case RT1718S_RT2_BC12_STAT_PORT_STATUS_DCP:
			return CHARGE_SUPPLIER_BC12_DCP;
	}

	return CHARGE_SUPPLIER_OTHER;
}

static int rt1718s_get_bc12_ilim(enum charge_supplier supplier)
{
	switch (supplier) {
	case CHARGE_SUPPLIER_BC12_DCP:
	case CHARGE_SUPPLIER_BC12_CDP:
		return USB_CHARGER_MAX_CURR_MA;
	case CHARGE_SUPPLIER_BC12_SDP:
	default:
		return USB_CHARGER_MIN_CURR_MA;
	}
}

static void rt1718s_update_charge_manager(int port,
					  enum charge_supplier new_bc12_type)
{
	static enum charge_supplier current_bc12_type = CHARGE_SUPPLIER_NONE;

	if (new_bc12_type != current_bc12_type) {
		charge_manager_update_charge(current_bc12_type, port, NULL);

		if (new_bc12_type != CHARGE_SUPPLIER_NONE) {
			struct charge_port_info chg = {
				.current = rt1718s_get_bc12_ilim(new_bc12_type),
				.voltage = USB_CHARGER_VOLTAGE_MV,
			};

			charge_manager_update_charge(new_bc12_type, port, &chg);
		}

		current_bc12_type = new_bc12_type;
	}
}

static int rt1718s_bc12_ramp_allowed(int supplier)
{
	/* TODO */
	return false;
}

static void rt1718s_bc12_usb_charger_task(const int port)
{
	rt1718s_enable_bc12_sink(port, false);

	while (1) {
		uint32_t evt = task_wait_event(-1);

		if (evt & USB_CHG_EVENT_DR_UFP) {
			rt1718s_enable_bc12_sink(port, true);
		}

		if ((evt & USB_CHG_EVENT_DR_DFP) ||
		    (evt & USB_CHG_EVENT_CC_OPEN)) {
			rt1718s_update_charge_manager(
					port, CHARGE_SUPPLIER_NONE);
		}

		/* detection done, update charge_manager and stop detection */
		if (evt & USB_CHG_EVENT_BC12) {
			rt1718s_update_charge_manager(
					port, rt1718s_get_bc12_type(port));
			rt1718s_enable_bc12_sink(port, false);
		}
	}
}

void rt1718s_vendor_defined_alert(int port)
{
	int rv, value;
	
	/* Process BC12 alert */
	rv = rt1718s_read8(port, RT1718S_RT_INT6, &value);
	if (rv)
		return;
	
	/* clear BC12 alert */
	rv = rt1718s_write8(port, RT1718S_RT_INT6, value);
	if (rv)
		return;

	/* check snk done */
	if (value & RT1718S_RT_INT6_INT_BC12_SNK_DONE)
		task_set_event(USB_CHG_PORT_TO_TASK_ID(port),
			       USB_CHG_EVENT_BC12);

	/* HACK: clear vconn ov/oc */
	rt1718s_write8(port, 0x99, 0xFF);
	rt1718s_write8(port, 0x90, 0x87);
	rt1718s_write8(port, 0x11, 0x80);
}
static void rt1718s_alert(int port)
{
	int alert;

	tcpc_read16(port, TCPC_REG_ALERT, &alert);
	if (alert & TCPC_REG_ALERT_VENDOR_DEF)
		rt1718s_vendor_defined_alert(port);
	tcpci_tcpc_alert(port);
}

/* RT1718S is a TCPCI compatible port controller */
const struct tcpm_drv rt1718s_tcpm_drv = {
	.init			= &rt1718s_init,
	.release		= &tcpci_tcpm_release,
	.get_cc			= &tcpci_tcpm_get_cc,
#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
	.check_vbus_level	= &tcpci_tcpm_check_vbus_level,
#endif
	.select_rp_value	= &tcpci_tcpm_select_rp_value,
	.set_cc			= &tcpci_tcpm_set_cc,
	.set_polarity		= &tcpci_tcpm_set_polarity,
#ifdef CONFIG_USB_PD_DECODE_SOP
	.sop_prime_enable	= &tcpci_tcpm_sop_prime_enable,
#endif
	.set_vconn		= &tcpci_tcpm_set_vconn,
	.set_msg_header		= &tcpci_tcpm_set_msg_header,
	.set_rx_enable		= &tcpci_tcpm_set_rx_enable,
	.get_message_raw	= &tcpci_tcpm_get_message_raw,
	.transmit		= &tcpci_tcpm_transmit,
	.tcpc_alert		= &rt1718s_alert,
#ifdef CONFIG_USB_PD_DISCHARGE_TCPC
	.tcpc_discharge_vbus	= &tcpci_tcpc_discharge_vbus,
#endif
#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
	.drp_toggle		= &tcpci_tcpc_drp_toggle,
#endif
	.get_chip_info		= &tcpci_get_chip_info,
#ifdef CONFIG_USB_PD_PPC
	.set_snk_ctrl		= &tcpci_tcpm_set_snk_ctrl,
	.set_src_ctrl		= &tcpci_tcpm_set_src_ctrl,
#endif
#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
	.enter_low_power_mode	= &tcpci_enter_low_power_mode,
#endif
};

const struct bc12_drv rt1718s_bc12_drv = {
	.usb_charger_task = rt1718s_bc12_usb_charger_task,
	.ramp_allowed = rt1718s_bc12_ramp_allowed,
};
