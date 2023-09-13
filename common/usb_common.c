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

<<<<<<< HEAD   (1ca04d charge_manager: Make charge OVERRIDE_DONT_CHARGE persistent)
=======
enum tcpc_cc_polarity get_src_polarity(enum tcpc_cc_voltage_status cc1,
				       enum tcpc_cc_voltage_status cc2)
{
	return (cc1 == TYPEC_CC_VOLT_RD) ? POLARITY_CC1 : POLARITY_CC2;
}

enum pd_cc_states pd_get_cc_state(enum tcpc_cc_voltage_status cc1,
				  enum tcpc_cc_voltage_status cc2)
{
	/* Port partner is a SNK */
	if (cc_is_snk_dbg_acc(cc1, cc2))
		return PD_CC_UFP_DEBUG_ACC;
	if (cc_is_at_least_one_rd(cc1, cc2))
		return PD_CC_UFP_ATTACHED;
	if (cc_is_audio_acc(cc1, cc2))
		return PD_CC_UFP_AUDIO_ACC;

	/* Port partner is a SRC */
	if (cc_is_rp(cc1) && cc_is_rp(cc2))
		return PD_CC_DFP_DEBUG_ACC;
	if (cc_is_rp(cc1) || cc_is_rp(cc2))
		return PD_CC_DFP_ATTACHED;

	/*
	 * 1) Both lines are Vopen or
	 * 2) Only an e-marked cabled without a partner on the other side
	 */
	return PD_CC_NONE;
}

__overridable int pd_board_check_request(uint32_t rdo, int pdo_cnt)
{
	return EC_SUCCESS;
}

int pd_get_source_pdo(const uint32_t **src_pdo_p, const int port)
{
#if defined(CONFIG_USB_PD_TCPMV2) && defined(CONFIG_USB_PE_SM)
	const uint32_t *src_pdo;
	const int pdo_cnt = dpm_get_source_pdo(&src_pdo, port);
#elif defined(CONFIG_USB_PD_DYNAMIC_SRC_CAP) || \
	defined(CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT)
	const uint32_t *src_pdo;
	const int pdo_cnt = charge_manager_get_source_pdo(&src_pdo, port);
#else
	const uint32_t *src_pdo = pd_src_pdo;
	const int pdo_cnt = pd_src_pdo_cnt;
#endif

	*src_pdo_p = src_pdo;
	return pdo_cnt;
}

int pd_check_requested_voltage(uint32_t rdo, const int port)
{
	int max_ma = rdo & 0x3FF;
	int op_ma = (rdo >> 10) & 0x3FF;
	int idx = RDO_POS(rdo);
	uint32_t pdo;
	uint32_t pdo_ma;
	const uint32_t *src_pdo;
	int pdo_cnt;

	pdo_cnt = pd_get_source_pdo(&src_pdo, port);

	/* Check for invalid index */
	if (!idx || idx > pdo_cnt)
		return EC_ERROR_INVAL;

	/* Board specific check for this request */
	if (pd_board_check_request(rdo, pdo_cnt))
		return EC_ERROR_INVAL;

	/* check current ... */
	pdo = src_pdo[idx - 1];
	pdo_ma = (pdo & 0x3ff);

	if (op_ma > pdo_ma)
		return EC_ERROR_INVAL; /* too much op current */

	if (max_ma > pdo_ma && !(rdo & RDO_CAP_MISMATCH))
		return EC_ERROR_INVAL; /* too much max current */

	CPRINTF("Requested %d mV %d mA (for %d/%d mA)\n",
		((pdo >> 10) & 0x3ff) * 50, (pdo & 0x3ff) * 10, op_ma * 10,
		max_ma * 10);

	/* Accept the requested voltage */
	return EC_SUCCESS;
}

__overridable uint8_t board_get_usb_pd_port_count(void)
{
	return CONFIG_USB_PD_PORT_MAX_COUNT;
}

__overridable bool board_is_usb_pd_port_present(int port)
{
	/*
	 * Use board_get_usb_pd_port_count() instead of checking
	 * CONFIG_USB_PD_PORT_MAX_COUNT directly here for legacy boards
	 * that implement board_get_usb_pd_port_count() but do not
	 * implement board_is_usb_pd_port_present().
	 */

	return (port >= 0) && (port < board_get_usb_pd_port_count());
}

__overridable bool board_is_dts_port(int port)
{
	return true;
}

int pd_get_retry_count(int port, enum tcpci_msg_type type)
{
	/* PD 3.0 6.7.7: nRetryCount = 2; PD 2.0 6.6.9: nRetryCount = 3 */
	return pd_get_rev(port, type) == PD_REV30 ? 2 : 3;
}

enum pd_drp_next_states drp_auto_toggle_next_state(
	uint64_t *drp_sink_time, enum pd_power_role power_role,
	enum pd_dual_role_states drp_state, enum tcpc_cc_voltage_status cc1,
	enum tcpc_cc_voltage_status cc2, bool auto_toggle_supported)
{
	const bool hardware_debounced_unattached =
		((drp_state == PD_DRP_TOGGLE_ON) && auto_toggle_supported);

	/* Set to appropriate port state */
	if (cc_is_open(cc1, cc2) || cc_is_pwred_cbl_without_snk(cc1, cc2)) {
		/*
		 * If nothing is attached then use drp_state to determine next
		 * state. If DRP auto toggle is still on, then remain in the
		 * DRP_AUTO_TOGGLE state. Otherwise, stop dual role toggling
		 * and go to a disconnected state.
		 */
		switch (drp_state) {
		case PD_DRP_TOGGLE_OFF:
			return DRP_TC_DEFAULT;
		case PD_DRP_FREEZE:
			if (power_role == PD_ROLE_SINK)
				return DRP_TC_UNATTACHED_SNK;
			else
				return DRP_TC_UNATTACHED_SRC;
		case PD_DRP_FORCE_SINK:
			return DRP_TC_UNATTACHED_SNK;
		case PD_DRP_FORCE_SOURCE:
			return DRP_TC_UNATTACHED_SRC;
		case PD_DRP_TOGGLE_ON:
		default:
			if (!auto_toggle_supported) {
				if (power_role == PD_ROLE_SINK)
					return DRP_TC_UNATTACHED_SNK;
				else
					return DRP_TC_UNATTACHED_SRC;
			}

			return DRP_TC_DRP_AUTO_TOGGLE;
		}
	} else if ((cc_is_rp(cc1) || cc_is_rp(cc2)) &&
		   drp_state != PD_DRP_FORCE_SOURCE) {
		/* SNK allowed unless ForceSRC */
		if (hardware_debounced_unattached)
			return DRP_TC_ATTACHED_WAIT_SNK;
		return DRP_TC_UNATTACHED_SNK;
	} else if (cc_is_at_least_one_rd(cc1, cc2) ||
		   cc_is_audio_acc(cc1, cc2)) {
		/*
		 * SRC allowed unless ForceSNK or Toggle Off
		 *
		 * Ideally we wouldn't use auto-toggle when drp_state is
		 * TOGGLE_OFF/FORCE_SINK, but for some TCPCs, auto-toggle can't
		 * be prevented in low power mode. Try being a sink in case the
		 * connected device is dual-role (this ensures reliable charging
		 * from a hub, b/72007056). 100 ms is enough time for a
		 * dual-role partner to switch from sink to source. If the
		 * connected device is sink-only, then we will attempt
		 * TC_UNATTACHED_SNK twice (due to debounce time), then return
		 * to low power mode (and stay there). After 200 ms, reset
		 * ready for a new connection.
		 */
		if (drp_state == PD_DRP_TOGGLE_OFF ||
		    drp_state == PD_DRP_FORCE_SINK) {
			if (get_time().val > *drp_sink_time + 200 * MSEC)
				*drp_sink_time = get_time().val;
			if (get_time().val < *drp_sink_time + 100 * MSEC)
				return DRP_TC_UNATTACHED_SNK;
			else
				return DRP_TC_DRP_AUTO_TOGGLE;
		} else {
			if (hardware_debounced_unattached)
				return DRP_TC_ATTACHED_WAIT_SRC;
			return DRP_TC_UNATTACHED_SRC;
		}
	} else {
		/* Anything else, keep toggling */
		if (!auto_toggle_supported) {
			if (power_role == PD_ROLE_SINK)
				return DRP_TC_UNATTACHED_SNK;
			else
				return DRP_TC_UNATTACHED_SRC;
		}

		return DRP_TC_DRP_AUTO_TOGGLE;
	}
}

__overridable bool usb_ufp_check_usb3_enable(int port)
{
	return false;
}

mux_state_t get_mux_mode_to_set(int port)
{
	/*
	 * If the SoC is down, then we disconnect the MUX to save power since
	 * no one cares about the data lines.
	 */
	if (IS_ENABLED(CONFIG_AP_POWER_CONTROL) &&
	    chipset_in_or_transitioning_to_state(CHIPSET_STATE_ANY_OFF))
		return USB_PD_MUX_NONE;

	/*
	 * When PD stack is disconnected, then mux should be disconnected, which
	 * is also what happens in the set_state disconnection code. Once the
	 * PD state machine progresses out of disconnect, the MUX state will
	 * be set correctly again.
	 */
	if (pd_is_disconnected(port))
		return USB_PD_MUX_NONE;

	/*
	 * For type-c only connections, there may be a need to enable USB3.1
	 * mode when the port is in a UFP data role, independent of any other
	 * conditions which are checked below. The default function returns
	 * false, so only boards that override this check will be affected.
	 */
	if (usb_ufp_check_usb3_enable(port) &&
	    pd_get_data_role(port) == PD_ROLE_UFP)
		return USB_PD_MUX_USB_ENABLED;

	/* If new data role isn't DFP & we only support DFP, also disconnect. */
	if (IS_ENABLED(CONFIG_USB_PD_DUAL_ROLE) &&
	    IS_ENABLED(CONFIG_USBC_SS_MUX_DFP_ONLY) &&
	    pd_get_data_role(port) != PD_ROLE_DFP)
		return USB_PD_MUX_NONE;

	/* If new data role isn't UFP & we only support UFP then disconnect. */
	if (IS_ENABLED(CONFIG_USB_PD_DUAL_ROLE) &&
	    IS_ENABLED(CONFIG_USBC_SS_MUX_UFP_ONLY) &&
	    pd_get_data_role(port) != PD_ROLE_UFP)
		return USB_PD_MUX_NONE;

	/* Otherwise connect mux since we are in S3+ */
	return USB_PD_MUX_USB_ENABLED;
}

void set_usb_mux_with_current_data_role(int port)
{
	if (IS_ENABLED(CONFIG_USBC_SS_MUX)) {
		mux_state_t mux_mode = get_mux_mode_to_set(port);
		enum usb_switch usb_switch_mode =
			(mux_mode == USB_PD_MUX_NONE) ? USB_SWITCH_DISCONNECT :
							USB_SWITCH_CONNECT;

		usb_mux_set(port, mux_mode, usb_switch_mode,
			    polarity_rm_dts(pd_get_polarity(port)));
	}
}

void usb_mux_set_safe_mode(int port)
{
	if (IS_ENABLED(CONFIG_USBC_SS_MUX)) {
		usb_mux_set(port, USB_PD_MUX_SAFE_MODE, USB_SWITCH_CONNECT,
			    polarity_rm_dts(pd_get_polarity(port)));
	}

	/* Isolate the SBU lines. */
	typec_set_sbu(port, false);
}

void usb_mux_set_safe_mode_exit(int port)
{
	if (IS_ENABLED(CONFIG_USBC_SS_MUX))
		usb_mux_set(port, USB_PD_MUX_NONE, USB_SWITCH_CONNECT,
			    polarity_rm_dts(pd_get_polarity(port)));

	/* Isolate the SBU lines. */
	typec_set_sbu(port, false);
}

void pd_send_hard_reset(int port)
{
	task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_SEND_HARD_RESET);
}

#ifdef CONFIG_USBC_OCP
void pd_handle_overcurrent(int port)
{
	if ((port < 0) || (port >= board_get_usb_pd_port_count())) {
		CPRINTS("%s(%d) Invalid port!", __func__, port);
		return;
	}

	CPRINTS("C%d: overcurrent!", port);

	if (IS_ENABLED(CONFIG_USB_PD_LOGGING))
		pd_log_event(PD_EVENT_PS_FAULT, PD_LOG_PORT_SIZE(port, 0),
			     PS_FAULT_OCP, NULL);

	/* No action to take if disconnected, just log. */
	if (pd_is_disconnected(port))
		return;

	/*
	 * Keep track of the overcurrent events and allow the module to perform
	 * the spec-dictated recovery actions.
	 */
	usbc_ocp_add_event(port);
}

#endif /* CONFIG_USBC_OCP */

__maybe_unused void pd_handle_cc_overvoltage(int port)
{
	pd_send_hard_reset(port);
}
>>>>>>> CHANGE (6c2f1d tcpmv2: Connect SuperSpeed on Attached.{SRC,SNK})

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
	/* make DP interface safe until configure */
	dp_flags[port] = 0;
	dp_status[port] = 0;
	usb_mux_set(port,
#ifdef CONFIG_USB_MUX_VIRTUAL
		TYPEC_MUX_SAFE,
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
