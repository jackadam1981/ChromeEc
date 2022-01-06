/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Contains common USB functions shared between the old (i.e. usb_pd_protocol)
 * and the new (i.e. usb_sm_*) USB-C PD stacks.
 */

#include "chipset.h"
#include "common.h"
#include "charge_state.h"
#include "system.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#else
#define CPRINTS(format, args...)
#define CPRINTF(format, args...)
#endif

int usb_get_battery_soc(void)
{
#if defined(CONFIG_CHARGER)
	return charge_get_percent();
#elif defined(CONFIG_BATTERY)
	return board_get_battery_soc();
#else
	return 0;
#endif
}

/*
 * CC values for regular sources and Debug sources (aka DTS)
 *
 * Source type  Mode of Operation   CC1    CC2
 * ---------------------------------------------
 * Regular      Default USB Power   RpUSB  Open
 * Regular      USB-C @ 1.5 A       Rp1A5  Open
 * Regular      USB-C @ 3 A	    Rp3A0  Open
 * DTS		Default USB Power   Rp3A0  Rp1A5
 * DTS		USB-C @ 1.5 A       Rp1A5  RpUSB
 * DTS		USB-C @ 3 A	    Rp3A0  RpUSB
 */

typec_current_t usb_get_typec_current_limit(enum pd_cc_polarity_type polarity,
	enum tcpc_cc_voltage_status cc1, enum tcpc_cc_voltage_status cc2)
{
	typec_current_t charge = 0;
	enum tcpc_cc_voltage_status cc = polarity ? cc2 : cc1;
	enum tcpc_cc_voltage_status cc_alt = polarity ? cc1 : cc2;

	switch (cc) {
	case TYPEC_CC_VOLT_RP_3_0:
		if (!cc_is_rp(cc_alt) || cc_alt == TYPEC_CC_VOLT_RP_DEF)
			charge = 3000;
		else if (cc_alt == TYPEC_CC_VOLT_RP_1_5)
			charge = 500;
		break;
	case TYPEC_CC_VOLT_RP_1_5:
		charge = 1500;
		break;
	case TYPEC_CC_VOLT_RP_DEF:
		charge = 500;
		break;
	default:
		break;
	}

#ifdef CONFIG_USBC_DISABLE_CHARGE_FROM_RP_DEF
	if (charge == 500)
		charge = 0;
#endif

	if (cc_is_rp(cc_alt))
		charge |= TYPEC_CURRENT_DTS_MASK;

	return charge;
}

enum pd_cc_polarity_type get_snk_polarity(enum tcpc_cc_voltage_status cc1,
	enum tcpc_cc_voltage_status cc2)
{
	/* The following assumes:
	 *
	 * TYPEC_CC_VOLT_RP_3_0 > TYPEC_CC_VOLT_RP_1_5
	 * TYPEC_CC_VOLT_RP_1_5 > TYPEC_CC_VOLT_RP_DEF
	 * TYPEC_CC_VOLT_RP_DEF > TYPEC_CC_VOLT_OPEN
	 */
	return cc2 > cc1;
}


__overridable int pd_board_checks(void)
{
	return EC_SUCCESS;
}

__overridable int pd_check_data_swap(int port, int data_role)
{
	/* Allow data swap if we are a UFP, otherwise don't allow. */
	return (data_role == PD_ROLE_UFP) ? 1 : 0;
}

__overridable void pd_check_dr_role(int port, int dr_role, int flags)
{
	/* If UFP, try to switch to DFP */
	if ((flags & PD_FLAGS_PARTNER_DR_DATA) && dr_role == PD_ROLE_UFP)
		pd_request_data_swap(port);
}

__overridable int pd_check_power_swap(int port)
{
	/*
	 * Allow power swap if we are acting as a dual role device.  If we are
	 * not acting as dual role (ex. suspended), then only allow power swap
	 * if we are sourcing when we could be sinking.
	 */
	if (pd_get_dual_role(port) == PD_DRP_TOGGLE_ON)
		return 1;
	else if (pd_get_role(port) == PD_ROLE_SOURCE)
		return 1;

	return 0;
}

__overridable void pd_check_pr_role(int port, int pr_role, int flags)
{
	/*
	 * If partner is dual-role power and dualrole toggling is on, consider
	 * if a power swap is necessary.
	 */
	if ((flags & PD_FLAGS_PARTNER_DR_POWER) &&
	    pd_get_dual_role(port) == PD_DRP_TOGGLE_ON) {
		/*
		 * If we are a sink and partner is not externally powered, then
		 * swap to become a source. If we are source and partner is
		 * externally powered, swap to become a sink.
		 */
		int partner_extpower = flags & PD_FLAGS_PARTNER_EXTPOWER;

		if ((!partner_extpower && pr_role == PD_ROLE_SINK) ||
		     (partner_extpower && pr_role == PD_ROLE_SOURCE))
			pd_request_power_swap(port);
	}
}

__overridable void pd_execute_data_swap(int port, int data_role)
{
}

__overridable int pd_is_valid_input_voltage(int mv)
{
	return 1;
}

__overridable void pd_transition_voltage(int idx)
{
	/* Most devices are fixed 5V output. */
}

__overridable void typec_set_source_current_limit(int p, enum tcpc_rp_value rp)
{
#ifdef CONFIG_USBC_PPC
	ppc_set_vbus_source_current_limit(p, rp);
#endif
}

/* ---------------- Power Data Objects (PDOs) ----------------- */
#ifndef CONFIG_USB_PD_CUSTOM_PDO
#define PDO_FIXED_FLAGS (PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP |\
			 PDO_FIXED_COMM_CAP)

const uint32_t pd_src_pdo[] = {
	PDO_FIXED(5000, 1500, PDO_FIXED_FLAGS),
};
const int pd_src_pdo_cnt = ARRAY_SIZE(pd_src_pdo);
const uint32_t pd_src_pdo_max[] = {
	PDO_FIXED(5000, 3000, PDO_FIXED_FLAGS),
};
const int pd_src_pdo_max_cnt = ARRAY_SIZE(pd_src_pdo_max);

const uint32_t pd_snk_pdo[] = {
	PDO_FIXED(5000, 500, PDO_FIXED_FLAGS),
	PDO_BATT(4750, PD_MAX_VOLTAGE_MV, PD_OPERATING_POWER_MW),
	PDO_VAR(4750, PD_MAX_VOLTAGE_MV, PD_MAX_CURRENT_MA),
};
const int pd_snk_pdo_cnt = ARRAY_SIZE(pd_snk_pdo);
#endif /* CONFIG_USB_PD_CUSTOM_PDO */

/* ----------------- Vendor Defined Messages ------------------ */
__overridable int pd_custom_vdm(int port, int cnt, uint32_t *payload,
				uint32_t **rpayload)
{
	int cmd = PD_VDO_CMD(payload[0]);
	uint16_t dev_id = 0;
	int is_rw, is_latest;

	/* make sure we have some payload */
	if (cnt == 0)
		return 0;

	switch (cmd) {
	case VDO_CMD_VERSION:
		/* guarantee last byte of payload is null character */
		*(payload + cnt - 1) = 0;
		CPRINTF("version: %s\n", (char *)(payload+1));
		break;
	case VDO_CMD_READ_INFO:
	case VDO_CMD_SEND_INFO:
		/* copy hash */
		if (cnt == 7) {
			dev_id = VDO_INFO_HW_DEV_ID(payload[6]);
			is_rw = VDO_INFO_IS_RW(payload[6]);

			is_latest = pd_dev_store_rw_hash(port,
							 dev_id,
							 payload + 1,
							 is_rw ?
							 SYSTEM_IMAGE_RW :
							 SYSTEM_IMAGE_RO);

			/*
			 * Send update host event unless our RW hash is
			 * already known to be the latest update RW.
			 */
			if (!is_rw || !is_latest)
				pd_send_host_event(PD_EVENT_UPDATE_DEVICE);

			CPRINTF("DevId:%d.%d SW:%d RW:%d\n",
				HW_DEV_ID_MAJ(dev_id),
				HW_DEV_ID_MIN(dev_id),
				VDO_INFO_SW_DBG_VER(payload[6]),
				is_rw);
		} else if (cnt == 6) {
			/* really old devices don't have last byte */
			pd_dev_store_rw_hash(port, dev_id, payload + 1,
					     SYSTEM_IMAGE_UNKNOWN);
		}
		break;
	case VDO_CMD_CURRENT:
		CPRINTF("Current: %dmA\n", payload[1]);
		break;
	case VDO_CMD_FLIP:
#ifdef CONFIG_USBC_SS_MUX
		usb_mux_flip(port);
#endif
		break;
#ifdef CONFIG_USB_PD_LOGGING
	case VDO_CMD_GET_LOG:
		pd_log_recv_vdm(port, cnt, payload);
		break;
#endif /* CONFIG_USB_PD_LOGGING */
	}

	return 0;
}

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
__overridable const struct svdm_response svdm_rsp = {
	.identity = NULL,
	.svids = NULL,
	.modes = NULL,
};

int dp_flags[CONFIG_USB_PD_PORT_COUNT];
uint32_t dp_status[CONFIG_USB_PD_PORT_COUNT];

__overridable void svdm_safe_dp_mode(int port)
{
<<<<<<< HEAD   (8c53b8 battery: Set EC_BATT_FLAG_INVALID_DATA correctly)
	/* make DP interface safe until configure */
	dp_flags[port] = 0;
	dp_status[port] = 0;
	usb_mux_set(port,
#ifdef CONFIG_USB_MUX_VIRTUAL
		TYPEC_MUX_SAFE,
=======
	return false;
}

/* VDM utility functions */
static void pd_usb_billboard_deferred(void)
{
	if (IS_ENABLED(CONFIG_USB_PD_ALT_MODE) &&
		!IS_ENABLED(CONFIG_USB_PD_ALT_MODE_DFP) &&
		!IS_ENABLED(CONFIG_USB_PD_SIMPLE_DFP) &&
		IS_ENABLED(CONFIG_USB_BOS)) {
		/*
		 * TODO(tbroch)
		 * 1. Will we have multiple type-C port UFPs
		 * 2. Will there be other modes applicable to DFPs besides DP
		 */
		if (!pd_alt_mode(0, TCPCI_MSG_SOP, USB_SID_DISPLAYPORT))
			usb_connect();
	}
}
DECLARE_DEFERRED(pd_usb_billboard_deferred);

#ifdef CONFIG_USB_PD_DISCHARGE
static void gpio_discharge_vbus(int port, int enable)
{
#ifdef CONFIG_USB_PD_DISCHARGE_GPIO
	enum gpio_signal dischg_gpio[] = {
		GPIO_USB_C0_DISCHARGE,
#if CONFIG_USB_PD_PORT_MAX_COUNT > 1
		GPIO_USB_C1_DISCHARGE,
#endif
#if CONFIG_USB_PD_PORT_MAX_COUNT > 2
		GPIO_USB_C2_DISCHARGE,
#endif
	};
	BUILD_ASSERT(ARRAY_SIZE(dischg_gpio) == CONFIG_USB_PD_PORT_MAX_COUNT);

	gpio_set_level(dischg_gpio[port], enable);
#endif /* CONFIG_USB_PD_DISCHARGE_GPIO */
}

void pd_set_vbus_discharge(int port, int enable)
{
	static mutex_t discharge_lock[CONFIG_USB_PD_PORT_MAX_COUNT];
#ifdef CONFIG_ZEPHYR
	static bool inited[CONFIG_USB_PD_PORT_MAX_COUNT];

	if (!inited[port]) {
		(void)k_mutex_init(&discharge_lock[port]);
		inited[port] = true;
	}
#endif
	if (port >= board_get_usb_pd_port_count())
		return;

	mutex_lock(&discharge_lock[port]);
	enable &= !board_vbus_source_enabled(port);

	if (get_usb_pd_discharge() == USB_PD_DISCHARGE_GPIO) {
		gpio_discharge_vbus(port, enable);
	} else if (get_usb_pd_discharge() == USB_PD_DISCHARGE_TCPC) {
#ifdef CONFIG_USB_PD_DISCHARGE_TCPC
		tcpc_discharge_vbus(port, enable);
#endif
	} else if (get_usb_pd_discharge() == USB_PD_DISCHARGE_PPC) {
#ifdef CONFIG_USB_PD_DISCHARGE_PPC
		ppc_discharge_vbus(port, enable);
#endif
	}

	mutex_unlock(&discharge_lock[port]);
}
#endif /* CONFIG_USB_PD_DISCHARGE */

#ifdef CONFIG_USB_PD_TCPM_TCPCI
static atomic_t pd_ports_to_resume;
static void resume_pd_port(void)
{
	uint32_t port;
	uint32_t suspended_ports = atomic_clear(&pd_ports_to_resume);

	while (suspended_ports) {
		port = __builtin_ctz(suspended_ports);
		suspended_ports &= ~BIT(port);
		pd_set_suspend(port, 0);
	}
}
DECLARE_DEFERRED(resume_pd_port);

void pd_deferred_resume(int port)
{
	atomic_or(&pd_ports_to_resume, 1 << port);
	hook_call_deferred(&resume_pd_port_data, 5 * SECOND);
}
#endif /* CONFIG_USB_PD_TCPM_TCPCI */

__overridable int pd_snk_is_vbus_provided(int port)
{
	return EC_SUCCESS;
}

/*
 * Check the specified Vbus level
 *
 * Note that boards may override this function if they have a method outside the
 * TCPCI driver to verify vSafe0V.
 */
__overridable bool pd_check_vbus_level(int port, enum vbus_level level)
{
	if (IS_ENABLED(CONFIG_USB_PD_VBUS_DETECT_TCPC) &&
		(get_usb_pd_vbus_detect() == USB_PD_VBUS_DETECT_TCPC)) {
		return tcpm_check_vbus_level(port, level);
	}
	else if (level == VBUS_PRESENT)
		return pd_snk_is_vbus_provided(port);
	else
		return !pd_snk_is_vbus_provided(port);
}

int pd_is_vbus_present(int port)
{
	return pd_check_vbus_level(port, VBUS_PRESENT);
}

#ifdef CONFIG_USB_PD_FRS
__overridable int board_pd_set_frs_enable(int port, int enable)
{
	return EC_SUCCESS;
}

int pd_set_frs_enable(int port, int enable)
{
	int rv = EC_SUCCESS;

	if (IS_ENABLED(CONFIG_USB_PD_FRS_PPC))
		rv = ppc_set_frs_enable(port, enable);
	if (rv == EC_SUCCESS && IS_ENABLED(CONFIG_USB_PD_FRS_TCPC))
		rv = tcpm_set_frs_enable(port, enable);
	if (rv == EC_SUCCESS)
		rv = board_pd_set_frs_enable(port, enable);
	return rv;
}
#endif /* defined(CONFIG_USB_PD_FRS) */

#ifdef CONFIG_CMD_TCPC_DUMP
/*
 * Dump TCPC registers.
 */
void tcpc_dump_registers(int port, const struct tcpc_reg_dump_map *reg,
			  int count)
{
	int i, val;

	for (i = 0; i < count; i++, reg++) {
		switch (reg->size) {
		case 1:
			tcpc_read(port, reg->addr, &val);
			ccprintf("  %-30s(0x%02x) =   0x%02x\n",
				reg->name, reg->addr, (uint8_t)val);
			break;
		case 2:
			tcpc_read16(port, reg->addr, &val);
			ccprintf("  %-30s(0x%02x) = 0x%04x\n",
				reg->name, reg->addr, (uint16_t)val);
			break;
		}
		cflush();
	}

}

static int command_tcpc_dump(int argc, char **argv)
{
	int port;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	port = atoi(argv[1]);
	if ((port < 0) || (port >= board_get_usb_pd_port_count())) {
		CPRINTS("%s(%d) Invalid port!", __func__, port);
		return EC_ERROR_INVAL;
	}
	/* Dump TCPC registers. */
	tcpm_dump_registers(port);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(tcpci_dump, command_tcpc_dump, "<Type-C port>",
			"dump the TCPC regs");
#endif /* defined(CONFIG_CMD_TCPC_DUMP) */

void pd_srccaps_dump(int port)
{
	int i;
	const uint32_t *const srccaps = pd_get_src_caps(port);

	for (i = 0; i < pd_get_src_cap_cnt(port); ++i) {
		uint32_t max_ma, max_mv, min_mv;

#ifdef CONFIG_CMD_PD_SRCCAPS_REDUCED_SIZE
		pd_extract_pdo_power(srccaps[i], &max_ma, &max_mv, &min_mv);

		if ((srccaps[i] & PDO_TYPE_MASK) == PDO_TYPE_AUGMENTED) {
			if (IS_ENABLED(CONFIG_USB_PD_REV30))
				ccprintf("%d: %dmV-%dmV/%dmA\n", i, min_mv,
					 max_mv, max_ma);
		} else {
			ccprintf("%d: %dmV/%dmA\n", i, max_mv, max_ma);
		}
>>>>>>> CHANGE (e296ef usb_common: Fix CONFIG_USB_PD_DISCHARGE_TCPC typo)
#else
		TYPEC_MUX_NONE,
#endif
		USB_SWITCH_CONNECT, pd_get_polarity(port));

	/* Isolate the SBU lines. */
#ifdef CONFIG_USBC_PPC_SBU
	ppc_set_sbu(port, 0);
#endif
}

__overridable int svdm_enter_dp_mode(int port, uint32_t mode_caps)
{
	/*
	 * Don't enter the mode if the SoC is off.
	 *
	 * There's no need to enter the mode while the SoC is off; we'll
	 * actually enter the mode on the chipset resume hook.  Entering DP Alt
	 * Mode twice will confuse some monitors and require and unplug/replug
	 * to get them to work again.  The DP Alt Mode on USB-C spec says that
	 * if we don't need to maintain HPD connectivity info in a low power
	 * mode, then we shall exit DP Alt Mode.  (This is why we don't enter
	 * when the SoC is off as opposed to suspend where adding a display
	 * could cause a wake up.)
	 */
	if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
		return -1;

	/* Only enter mode if device is DFP_D capable */
	if (mode_caps & MODE_DP_SNK) {
		svdm_safe_dp_mode(port);

#ifdef CONFIG_MKBP_EVENT
		if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND))
			/*
			 * Wake the system up since we're entering DP AltMode.
			 */
			pd_notify_dp_alt_mode_entry();
#endif

		return 0;
	}

	return -1;
}

__overridable int svdm_dp_status(int port, uint32_t *payload)
{
	int opos = pd_alt_mode(port, USB_SID_DISPLAYPORT);

	payload[0] = VDO(USB_SID_DISPLAYPORT, 1,
			 CMD_DP_STATUS | VDO_OPOS(opos));
	payload[1] = VDO_DP_STATUS(0, /* HPD IRQ  ... not applicable */
				   0, /* HPD level ... not applicable */
				   0, /* exit DP? ... no */
				   0, /* usb mode? ... no */
				   0, /* multi-function ... no */
				   (!!(dp_flags[port] & DP_FLAGS_DP_ON)),
				   0, /* power low? ... no */
				   (!!DP_FLAGS_DP_ON));
	return 2;
};

__overridable int svdm_dp_config(int port, uint32_t *payload)
{
	int opos = pd_alt_mode(port, USB_SID_DISPLAYPORT);
	int mf_pref = PD_VDO_DPSTS_MF_PREF(dp_status[port]);
	int pin_mode = pd_dfp_dp_get_pin_mode(port, dp_status[port]);
	enum typec_mux mux_mode;

	if (!pin_mode)
		return 0;

	/*
	 * Multi-function operation is only allowed if that pin config is
	 * supported.
	 */
	mux_mode = ((pin_mode & MODE_DP_PIN_MF_MASK) && mf_pref) ?
		TYPEC_MUX_DOCK : TYPEC_MUX_DP;
	CPRINTS("pin_mode: %x, mf: %d, mux: %d", pin_mode, mf_pref, mux_mode);

	/* Connect the SBU and USB lines to the connector. */
#ifdef CONFIG_USBC_PPC_SBU
	ppc_set_sbu(port, 1);
#endif
	usb_mux_set(port, mux_mode, USB_SWITCH_CONNECT, pd_get_polarity(port));

	payload[0] = VDO(USB_SID_DISPLAYPORT, 1,
			 CMD_DP_CONFIG | VDO_OPOS(opos));
	payload[1] = VDO_DP_CFG(pin_mode,      /* pin mode */
				1,	       /* DPv1.3 signaling */
				2);	       /* UFP connected */
	return 2;
};

/*
 * timestamp of the next possible toggle to ensure the 2-ms spacing
 * between IRQ_HPD.
 */
#ifdef CONFIG_USB_PD_DP_HPD_GPIO
static
#endif
	uint64_t hpd_deadline[CONFIG_USB_PD_PORT_COUNT];
#ifndef PORT_TO_HPD
#define PORT_TO_HPD(port) ((port) ? GPIO_USB_C1_DP_HPD : GPIO_USB_C0_DP_HPD)
#endif /* PORT_TO_HPD */

__overridable void svdm_dp_post_config(int port)
{
	const struct usb_mux *mux = &usb_muxes[port];

	dp_flags[port] |= DP_FLAGS_DP_ON;
	if (!(dp_flags[port] & DP_FLAGS_HPD_HI_PENDING))
		return;

#ifdef CONFIG_USB_PD_DP_HPD_GPIO
	gpio_set_level(PORT_TO_HPD(port), 1);

	/* set the minimum time delay (2ms) for the next HPD IRQ */
	hpd_deadline[port] = get_time().val + HPD_USTREAM_DEBOUNCE_LVL;
#endif /* CONFIG_USB_PD_DP_HPD_GPIO */

	if (mux->hpd_update)
		mux->hpd_update(port, 1, 0);

#ifdef USB_PD_PORT_TCPC_MST
	if (port == USB_PD_PORT_TCPC_MST)
		baseboard_mst_enable_control(port, 1);
#endif
}

__overridable int svdm_dp_attention(int port, uint32_t *payload)
{
	int lvl = PD_VDO_DPSTS_HPD_LVL(payload[1]);
	int irq = PD_VDO_DPSTS_HPD_IRQ(payload[1]);
	const struct usb_mux *mux = &usb_muxes[port];
#ifdef CONFIG_USB_PD_DP_HPD_GPIO
	enum gpio_signal hpd = PORT_TO_HPD(port);
	int cur_lvl = gpio_get_level(hpd);
#endif /* CONFIG_USB_PD_DP_HPD_GPIO */

	dp_status[port] = payload[1];

	if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND) &&
	    (irq || lvl)) {
		/*
		 * Wake up the AP.  IRQ or level high indicates a DP sink is now
		 * present.
		 */
#ifdef CONFIG_MKBP_EVENT
		pd_notify_dp_alt_mode_entry();
#endif
	}

	/* Its initial DP status message prior to config */
	if (!(dp_flags[port] & DP_FLAGS_DP_ON)) {
		if (lvl)
			dp_flags[port] |= DP_FLAGS_HPD_HI_PENDING;
		return 1;
	}

#ifdef CONFIG_USB_PD_DP_HPD_GPIO
	if (irq & cur_lvl) {
		uint64_t now = get_time().val;
		/* wait for the minimum spacing between IRQ_HPD if needed */
		if (now < hpd_deadline[port])
			usleep(hpd_deadline[port] - now);

		/* generate IRQ_HPD pulse */
		gpio_set_level(hpd, 0);
		usleep(HPD_DSTREAM_DEBOUNCE_IRQ);
		gpio_set_level(hpd, 1);

		/* set the minimum time delay (2ms) for the next HPD IRQ */
		hpd_deadline[port] = get_time().val + HPD_USTREAM_DEBOUNCE_LVL;
	} else if (irq & !lvl) {
		/*
		 * IRQ can only be generated when the level is high, because
		 * the IRQ is signaled by a short low pulse from the high level.
		 */
		CPRINTF("ERR:HPD:IRQ&LOW\n");
		return 0; /* nak */
	} else {
		gpio_set_level(hpd, lvl);
		/* set the minimum time delay (2ms) for the next HPD IRQ */
		hpd_deadline[port] = get_time().val + HPD_USTREAM_DEBOUNCE_LVL;
	}
#endif /* CONFIG_USB_PD_DP_HPD_GPIO */

	if (mux->hpd_update)
		mux->hpd_update(port, lvl, irq);

#ifdef USB_PD_PORT_TCPC_MST
	if (port == USB_PD_PORT_TCPC_MST)
		baseboard_mst_enable_control(port, lvl);
#endif

	/* ack */
	return 1;
}

__overridable void svdm_exit_dp_mode(int port)
{
	const struct usb_mux *mux = &usb_muxes[port];

	svdm_safe_dp_mode(port);
#ifdef CONFIG_USB_PD_DP_HPD_GPIO
	gpio_set_level(PORT_TO_HPD(port), 0);
#endif /* CONFIG_USB_PD_DP_HPD_GPIO */
	if (mux->hpd_update)
		mux->hpd_update(port, 0, 0);
#ifdef USB_PD_PORT_TCPC_MST
	if (port == USB_PD_PORT_TCPC_MST)
		baseboard_mst_enable_control(port, 0);
#endif
}

__overridable int svdm_enter_gfu_mode(int port, uint32_t mode_caps)
{
	/* Always enter GFU mode */
	return 0;
}

__overridable void svdm_exit_gfu_mode(int port)
{
}

__overridable int svdm_gfu_status(int port, uint32_t *payload)
{
	/*
	 * This is called after enter mode is successful, send unstructured
	 * VDM to read info.
	 */
	pd_send_vdm(port, USB_VID_GOOGLE, VDO_CMD_READ_INFO, NULL, 0);
	return 0;
}

__overridable int svdm_gfu_config(int port, uint32_t *payload)
{
	return 0;
}

__overridable int svdm_gfu_attention(int port, uint32_t *payload)
{
	return 0;
}

const struct svdm_amode_fx supported_modes[] = {
	{
		.svid = USB_SID_DISPLAYPORT,
		.enter = &svdm_enter_dp_mode,
		.status = &svdm_dp_status,
		.config = &svdm_dp_config,
		.post_config = &svdm_dp_post_config,
		.attention = &svdm_dp_attention,
		.exit = &svdm_exit_dp_mode,
	},

	{
		.svid = USB_VID_GOOGLE,
		.enter = &svdm_enter_gfu_mode,
		.status = &svdm_gfu_status,
		.config = &svdm_gfu_config,
		.attention = &svdm_gfu_attention,
		.exit = &svdm_exit_gfu_mode,
	}
};
const int supported_modes_cnt = ARRAY_SIZE(supported_modes);
#endif /* CONFIG_USB_PD_ALT_MODE_DFP */
