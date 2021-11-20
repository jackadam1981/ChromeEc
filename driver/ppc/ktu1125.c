/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Kinetic KTU1125 USB-C Power Path Controller */

#include "common.h"
#include "console.h"
#include "ktu1125.h"
#include "hooks.h"
#include "i2c.h"
#include "system.h"
#include "timer.h"
#include "usb_charge.h"
#include "usb_pd_tcpm.h"
#include "usb_pd.h"
#include "usbc_ppc.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

static uint32_t irq_pending; /* Bitmask of ports signaling an interrupt. */

static int read_reg(uint8_t port, int reg, int *regval)
{
	return i2c_read8(ppc_chips[port].i2c_port,
			 ppc_chips[port].i2c_addr_flags,
			 reg,
			 regval);
}

static int write_reg(uint8_t port, int reg, int regval)
{
	return i2c_write8(ppc_chips[port].i2c_port,
			  ppc_chips[port].i2c_addr_flags,
			  reg,
			  regval);
}

static int set_flags(const int port, const int addr, const int flags_to_set)
{
	int val, rv;

	rv = read_reg(port, addr, &val);
	if (rv)
		return rv;

	val |= flags_to_set;

	return write_reg(port, addr, val);
}

static int clr_flags(const int port, const int addr, const int flags_to_clear)
{
	int val, rv;

	rv = read_reg(port, addr, &val);
	if (rv)
		return rv;

	val &= ~flags_to_clear;

	return write_reg(port, addr, val);
}

static int set_field(const int port, const int addr, const int shift,
		     const int field_length, const int field_to_set)
{
	int val, rv, mask;

	mask = ((1 << field_length) - 1) << shift;
	val = (field_to_set << shift) & mask;

	rv = clr_flags(port, addr, mask);
	if (rv)
		return rv;

	return set_flags(port, addr, val);
}


#ifdef CONFIG_CMD_PPC_DUMP
static int ktu1125_dump(int port)
{
	int i;
	int data;
	const int i2c_port = ppc_chips[port].i2c_port;
	const uint16_t i2c_addr_flags = ppc_chips[port].i2c_addr_flags;

	for (i = KTU1125_ID; i <= KTU1125_INT_DATA; i++) {
		i2c_read8(i2c_port, i2c_addr_flags, i, &data);
		CPRINTF("REG %02Xh = 0x%02x\n", i, data);
	}

	cflush();
	return EC_SUCCESS;
}
#endif /* defined(CONFIG_CMD_PPC_DUMP) */

/* helper */
static int ktu1125_power_path_control(int port, int enable)
{
	int status = enable ? clr_flags(port, KTU1125_CTRL_SW_CFG,
					KTU1125_SW_AB_EN)
			    : set_flags(port, KTU1125_CTRL_SW_CFG,
					KTU1125_SW_AB_EN);

	if (status) {
		CPRINTS("ppc p%d: Failed to %s power path",
			port, enable ? "enable" : "disable");
	}

	return status;
}

/* helper */
static int ktu1125_sbu_control(int port, int enable)
{
	int status = enable ? clr_flags(port, KTU1125_CTRL_SW_CFG,
					KTU1125_SBU_SHUT)
			    : set_flags(port, KTU1125_CTRL_SW_CFG,
					KTU1125_SBU_SHUT);

	if (status) {
		CPRINTS("ppc p%d: Failed to %s sbu",
			port, enable ? "enable" : "disable");
	}

	return status;
}

static int ktu1125_init(int port)
{
	int regval;
	int status;
	const int i2c_port  = ppc_chips[port].i2c_port;
	const uint16_t i2c_addr_flags = ppc_chips[port].i2c_addr_flags;

	CPRINTF("\n\nDBGDBGDBGDBG KTU1125 init\n\n");

	/* Read and verify KTU1125 Vendor and Chip ID */
	status =  i2c_read8(i2c_port, i2c_addr_flags, KTU1125_ID, &regval);

	if (status) {
		ppc_prints("Failed to read device ID!", port);
		return status;
	}

	if (regval != KTU1125_VENDOR_DIE_IDS) {
		ppc_err_prints("KTU1125 ID mismatch!", port, regval);
		return regval;
	}

/* ************* why not using ktu1125_set_vbus_source_current_limit ????? */

#ifdef CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT
	/* Set the sourcing current limit value. */
	switch (CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT) {
	case TYPEC_RP_3A0:
		/* Set current limit to ~3A. */
		regval = KTU1125_SYSB_ILIM_3_30;
		break;

	case TYPEC_RP_1A5:
	default:
		/* Set current limit to ~1.5A. */
		regval = KTU1125_SYSB_ILIM_1_70;
		break;
	}
#else /* !defined(CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT) */
	/* Default SRC current limit to ~1.5A. */
	regval = KTU1125_SYSB_ILIM_1_70;
#endif /* defined(CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT) */

	status = set_field(port, KTU1125_SET_SW_CFG, KTU1125_SYSB_CLP_SHIFT,
			    KTU1125_SYSB_CLP_LEN, regval);
	if (status) {
		ppc_prints("Failed to set KTU1125_SET_SW_CFG!", port);
		return status;
	}

	/* Set Vbus OVP threshold to ~5V. */
	regval = KTU1125_SYSB_VLIM_6_00;
	status = set_field(port, KTU1125_SET_SW2_CFG, KTU1125_OVP_BUS_SHIFT,
			    KTU1125_OVP_BUS_LEN, regval);
	if (status) {
		ppc_prints("Failed to set VBUS OVP!", port);
		return status;
	}

	/* BM: set vbus discharge resistance KTU1125_DIS_RES */
	regval = KTU1125_SYSB_VLIM_6_00;
	status = set_field(port, KTU1125_SET_SW2_CFG, KTU1125_OVP_BUS_SHIFT,
			    KTU1125_OVP_BUS_LEN, regval);
	if (status) {
		ppc_prints("Failed to set VBUS OVP!", port);
		return status;
	}

/* BM: enable VCONN KTU1125_VCONN_EN */


	/* Set Vbus UVP threshold to ~2.75V. */


	/* Enable SBU Fets and set SNK path  */
	status = ktu1125_sbu_control(port, 1);
	if (status)
		return status;

	status = clr_flags(port, KTU1125_CTRL_SW_CFG, KTU1125_POW_MODE);
	if (status) {
		ppc_err_prints("Could not select SNK path", port, status);
		return status;
	}

	/*
	 * Indicate we are using PP2 configuration 2 and enable OVP comparator
	 * for CC lines.
	 *
	 * Also, turn off under-voltage protection for incoming Vbus as it would
	 * prevent us from enabling SNK path before we hibernate the ec. We
	 * need to enable the SNK path so USB power will assert ACOK and wake
	 * the EC up went inserting USB power. We always turn off under-voltage
	 * protection because the battery charger will boost the voltage up
	 * to the needed battery voltage either way (and it will have its own
	 * low voltage protection).
	 */


	/*
	 * Set analog current limit delay to 200 us for PP1,
	 * set 1000 us for PP2 for compatibility.
	 */

#ifdef CONFIG_USBC_PPC_VCONN
	/*
	 * Set the deglitch timeout on the Vconn current limit to 640us.  This
	 * improves compatibility with some USB C -> HDMI devices versus the
	 * reset default (20 us).
	 */



#endif /* CONFIG_USBC_PPC_VCONN */

	/*
	 * Turn off dead battery resistors, turn on CC FETs, and set the higher
	 * of the two VCONN current limits (min 0.6A).  Many VCONN accessories
	 * trip the default current limit of min 0.35A.
	 */

	/* Set ideal diode mode for both PP1 and PP2. */

	/*
	 * Set RCP voltage threshold to 3mV instead of 6mV default for the
	 * source path. This modification helps prevent false RCP triggers
	 * against certain port partners when VBUS is set to 20V.
	 */

	/* Turn off PP1 FET. */

	/*
	 * Don't proceed with the rest of initialization if we're sysjumping.
	 * We would have already done this before.
	 */
	if (system_jumped_late())
		return EC_SUCCESS;

	/*
	 * Clear the digital reset bit, and mask off and clear vSafe0V
	 * interrupts. Leave the dead battery mode bit unchanged since it
	 * is checked below.
	 */

	/*
	 * Before turning on the PP2 FET, mask off all unwanted interrupts and
	 * then clear all pending interrupts.
	 *
	 * TODO(aaboagye): Unmask fast-role swap events once fast-role swap is
	 * implemented in the PD stack.
	 */

	/* Enable PP1 overcurrent interrupts. */


	/* Enable overcurrent interrupts. */
	regval = KTU1125_SYSA_OCP;
	status = clr_flags(port, KTU1125_INTMASK_SNK, regval);
	if (status) {
		ppc_prints("Failed to write KTU1125_INTMASK_SNK!", port);
		return status;
	}

	regval =  KTU1125_VCONN_CLP | KTU1125_SYSB_OCP;
	status = clr_flags(port, KTU1125_INTMASK_SRC, regval);
	if (status) {
		ppc_prints("Failed to write KTU1125_INTMASK_SRC!", port);
		return status;
	}

	/* Enable and CC1/CC2 overvoltage interrupts */
	regval = KTU1125_CC1_OVP | KTU1125_CC2_OVP;
	status = clr_flags(port, KTU1125_INTMASK_DATA, regval);
	if (status) {
		ppc_prints("Failed to write KTU1125_INTMASK_DATA!", port);
		return status;
	}

#if defined(CONFIG_USB_PD_VBUS_DETECT_PPC) && defined(CONFIG_USB_CHARGER)
	/*  If PPC is being used to detect VBUS, enable VBUS interrupts.  */

#endif  /* CONFIG_USB_PD_VBUS_DETECT_PPC && CONFIG_USB_CHARGER  */

	/* Now clear any pending interrupts. */


	/*
	 * For PP2, check to see if we booted in dead battery mode.  If we
	 * booted in dead battery mode, the PP2 FET will already be enabled.
	 */

	return EC_SUCCESS;
}

#ifdef CONFIG_USB_PD_VBUS_DETECT_PPC
static int ktu1125_is_vbus_present(int port)
{
	int regval;
	int rv;

	rv = read_reg(port, KTU1125_MONITOR_SNK, &regval);
	if (rv) {
		ppc_err_prints("VBUS present error", port, rv);
		return 0;
	}

	return regval & KTU1125_SYSA_OK;
}
#endif /* defined(CONFIG_USB_PD_VBUS_DETECT_PPC) */

static int ktu1125_is_sourcing_vbus(int port)
{
	int regval;
	int rv;

	rv = read_reg(port, KTU1125_MONITOR_SRC, &regval);
	if (rv) {
		ppc_err_prints("Sourcing VBUS error", port, rv);
		return 0;
	}

	return regval & KTU1125_VBUS_OK;
}

#ifdef CONFIG_USBC_PPC_POLARITY
static int ktu1125_set_polarity(int port, int polarity)
{
	if (polarity) {
		/* CC2 active. */
		clr_flags(port, KTU1125_CTRL_SW_CFG, KTU1125_CC2S_VCONN);
		return set_flags(port, KTU1125_CTRL_SW_CFG, KTU1125_CC1S_VCONN);
	}

	/* else CC1 active. */
	clr_flags(port, KTU1125_CTRL_SW_CFG, KTU1125_CC1S_VCONN);
	return set_flags(port, KTU1125_CTRL_SW_CFG, KTU1125_CC2S_VCONN);
}
#endif

static int ktu1125_set_vbus_source_current_limit(int port,
						 enum tcpc_rp_value rp)
{
	int regval;
	int status;

	/*
	 * Note that we chose the lowest current limit setting that is just
	 * above indicated Rp value. This is because these are minimum values
	 * and we must be able to provide the current that we advertise.
	 */
	switch (rp) {
	case TYPEC_RP_3A0:
		regval = KTU1125_SYSB_ILIM_3_30;
		break;

	case TYPEC_RP_1A5:
		regval = KTU1125_SYSB_ILIM_1_70;
		break;

	case TYPEC_RP_USB:
	default:
		regval = KTU1125_SYSB_ILIM_0_6;
		break;
	};


	status = set_field(port, KTU1125_SET_SW_CFG, KTU1125_SYSB_CLP_SHIFT,
			    KTU1125_SYSB_CLP_LEN, regval);
	if (status)
		ppc_prints("Failed to set KTU1125_SET_SW_CFG!", port);

	return status;
}

static int ktu1125_discharge_vbus(int port, int enable)
{
	int status = enable ? set_flags(port, KTU1125_SET_SW2_CFG,
					KTU1125_VBUS_DIS_EN)
			    : clr_flags(port, KTU1125_SET_SW2_CFG,
					KTU1125_VBUS_DIS_EN);

	if (status) {
		CPRINTS("ppc p%d: Failed to %s vbus discharge",
			port, enable ? "enable" : "disable");
		return status;
	}

	return EC_SUCCESS;
}

static int ktu1125_enter_low_power_mode(int port)
{
	int rv;

	/* Turn off both SRC and SNK FETs */
	rv = ktu1125_power_path_control(port, 0);
	if (rv)
		return rv;

	/* Turn off Vconn power */
	rv = clr_flags(port, KTU1125_CTRL_SW_CFG, KTU1125_VCONN_EN);
	if (rv) {
		ppc_err_prints("Could not disable Vconn", port, rv);
		return rv;
	}

	/* Turn off SBU path */
	rv = ktu1125_sbu_control(port, 0);
	if (rv)
		return rv;

	return EC_SUCCESS;
}

#ifdef CONFIG_USBC_PPC_VCONN
static int ktu1125_set_vconn(int port, int enable)
{
	int status = enable ? set_flags(port, KTU1125_CTRL_SW_CFG,
					KTU1125_VCONN_EN)
			    : clr_flags(port, KTU1125_CTRL_SW_CFG,
					KTU1125_VCONN_EN |
					KTU1125_CC1S_VCONN |
					KTU1125_CC2S_VCONN);

	return status;
}
#endif

#ifdef CONFIG_USB_PD_FRS_PPC
static int ktu1125_set_frs_enable(int port, int enable)
{
	int status = enable ? set_flags(port, KTU1125_CTRL_SW_CFG,
					KTU1125_FRS_EN)
			    : clr_flags(port, KTU1125_CTRL_SW_CFG,
					KTU1125_FRS_EN);

	return status;
}
#endif

static int ktu1125_vbus_sink_enable(int port, int enable)
{
	/* Select active sink */
	int rv = clr_flags(port, KTU1125_CTRL_SW_CFG, KTU1125_POW_MODE);

	if (rv) {
		ppc_err_prints("Could not select SNK path", port, rv);
		return rv;
	}

	return ktu1125_power_path_control(port, enable);
}

static int ktu1125_vbus_source_enable(int port, int enable)
{
	/* Select active source */
	int rv = set_flags(port, KTU1125_CTRL_SW_CFG, KTU1125_POW_MODE);

	if (rv) {
		ppc_err_prints("Could not select SRC path", port, rv);
		return rv;
	}

	return ktu1125_power_path_control(port, enable);
}

#ifdef CONFIG_USBC_PPC_SBU
static int ktu1125_set_sbu(int port, int enable)
{
	return ktu1125_sbu_control(port, enable);
}
#endif /* CONFIG_USBC_PPC_SBU */

static void ktu1125_handle_interrupt(int port)
{
	int attempt = 0;

	/*
	 * KTU1135's /INT pin is level, so process interrupts until it
	 * deasserts if the chip has a dedicated interrupt pin.
	 */
#ifdef CONFIG_USBC_PPC_DEDICATED_INT
	while (ppc_get_alert_status(port))
#endif
	{
		int snk = 0;
		int src = 0;
		int data = 0;

		attempt++;

		if (attempt > 1)
			ppc_prints("Could not clear interrupts on first "
				"try, retrying", port);

		read_reg(port, KTU1125_INT_SNK, &snk);
		read_reg(port, KTU1125_INT_SRC, &src);
		read_reg(port, KTU1125_INT_DATA, &data);


		/* Notify the system about the overcurrent event. */
		if ((snk & KTU1125_SYSA_OCP) ||
		    (src & KTU1125_SYSB_OCP))
			pd_handle_overcurrent(port);

		/*
		 * VCONN may be latched off due to an overcurrent.  Indicate
		 * when the VCONN overcurrent happens.
		 */
		if (src & KTU1125_VCONN_CLP)
			ppc_prints("VCONN OC!", port);

		/* Notify the system about the CC overvoltage event. */
		if (data & KTU1125_CC1_OVP || data & KTU1125_CC2_OVP) {
			ppc_prints("CC OV!", port);
			pd_handle_cc_overvoltage(port);
		}

#if defined(CONFIG_USB_PD_VBUS_DETECT_PPC) && defined(CONFIG_USB_CHARGER)
		/* Inform other modules about VBUS level */
		if (/* TBD - (src & KTU1125_VBUS_OVP) ??? */)
			usb_charger_vbus_change(port,
						ktu1125_is_vbus_present(port));
#endif  /* CONFIG_USB_PD_VBUS_DETECT_PPC && CONFIG_USB_CHARGER */

	}
}

static void ktu1125_irq_deferred(void)
{
	int i;
	uint32_t pending = atomic_clear(&irq_pending);

	for (i = 0; i < board_get_usb_pd_port_count(); i++)
		if (BIT(i) & pending)
			ktu1125_handle_interrupt(i);
}
DECLARE_DEFERRED(ktu1125_irq_deferred);

void ktu1125_interrupt(int port)
{
	atomic_or(&irq_pending, BIT(port));
	hook_call_deferred(&ktu1125_irq_deferred_data, 0);
}

const struct ppc_drv ktu1125_drv = {
	.init = &ktu1125_init,
	.is_sourcing_vbus = &ktu1125_is_sourcing_vbus,
	.vbus_sink_enable = &ktu1125_vbus_sink_enable,
	.vbus_source_enable = &ktu1125_vbus_source_enable,
#ifdef CONFIG_USBC_PPC_POLARITY
	.set_polarity = &ktu1125_set_polarity,
#endif
	.set_vbus_source_current_limit = &ktu1125_set_vbus_source_current_limit,
	.discharge_vbus = &ktu1125_discharge_vbus,

	/**
	 * Inform the PPC of the device is connected or disconnected.
	 *
	 * @param port: The Type-C port number.
	 * @param dev: PPC_DEV_SNK if a sink is connected, PPC_DEV_SRC if a
	 *             source is connected, PPC_DEV_DISCONNECTED if the device
	 *             is disconnected.
	 * @return EC_SUCCESS on success, error otherwise.
	 */
/*	int (*dev_is_connected)(int port, enum ppc_device_role dev);
 */

#ifdef CONFIG_USBC_PPC_SBU
	.set_sbu = &ktu1125_set_sbu,
#endif
#ifdef CONFIG_USBC_PPC_VCONN
	.set_vconn = &ktu1125_set_vconn,
#endif
#ifdef CONFIG_USB_PD_FRS_PPC
	.set_frs_enable = &ktu1125_set_frs_enable,
#endif
#ifdef CONFIG_CMD_PPC_DUMP
	.reg_dump = &ktu1125_dump,
#endif
#ifdef CONFIG_USB_PD_VBUS_DETECT_PPC
	.is_vbus_present = &ktu1125_is_vbus_present,
#endif
	.enter_low_power_mode = &ktu1125_enter_low_power_mode,
	.interrupt = &ktu1125_interrupt,
};
