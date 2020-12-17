/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Device Policy Manager implementation
 * Refer to USB PD 3.0 spec, version 2.0, sections 8.2 and 8.3
 */

#include "charge_state.h"
#include "compile_time_macros.h"
#include "console.h"
#include "ec_commands.h"
#include "system.h"
#include "tcpm.h"
#include "usb_dp_alt_mode.h"
#include "usb_mode.h"
#include "usb_pd.h"
#include "usb_pd_dpm.h"
#include "usb_tbt_alt_mode.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#else
#define CPRINTF(format, args...)
#define CPRINTS(format, args...)
#endif

static struct {
	uint32_t flags;
} dpm[CONFIG_USB_PD_PORT_MAX_COUNT];

#define DPM_SET_FLAG(port, flag) atomic_or(&dpm[(port)].flags, (flag))
#define DPM_CLR_FLAG(port, flag) atomic_clear_bits(&dpm[(port)].flags, (flag))
#define DPM_CHK_FLAG(port, flag) (dpm[(port)].flags & (flag))

/* Flags for internal DPM state */
#define DPM_FLAG_MODE_ENTRY_DONE BIT(0)
#define DPM_FLAG_EXIT_REQUEST    BIT(1)
#define DPM_FLAG_ENTER_DP        BIT(2)
#define DPM_FLAG_ENTER_TBT       BIT(3)
#define DPM_FLAG_ENTER_USB4      BIT(4)

enum ec_status pd_request_enter_mode(int port, enum typec_mode mode)
{
	if (port >= board_get_usb_pd_port_count())
		return EC_RES_INVALID_PARAM;

	/* Only one enter request may be active at a time. */
	if (DPM_CHK_FLAG(port, DPM_FLAG_ENTER_DP |
				DPM_FLAG_ENTER_TBT |
				DPM_FLAG_ENTER_USB4))
		return EC_RES_BUSY;

	switch (mode) {
	case TYPEC_MODE_DP:
		DPM_SET_FLAG(port, DPM_FLAG_ENTER_DP);
		break;
#ifdef CONFIG_USB_PD_TBT_COMPAT_MODE
	case TYPEC_MODE_TBT:
		DPM_SET_FLAG(port, DPM_FLAG_ENTER_TBT);
		break;
#endif /* CONFIG_USB_PD_TBT_COMPAT_MODE */
#ifdef CONFIG_USB_PD_USB4
	case TYPEC_MODE_USB4:
		DPM_SET_FLAG(port, DPM_FLAG_ENTER_USB4);
		break;
#endif
	default:
		return EC_RES_INVALID_PARAM;
	}

	DPM_CLR_FLAG(port, DPM_FLAG_MODE_ENTRY_DONE);
	DPM_CLR_FLAG(port, DPM_FLAG_EXIT_REQUEST);

	return EC_RES_SUCCESS;
}

void dpm_init(int port)
{
	dpm[port].flags = 0;
}

static void dpm_set_mode_entry_done(int port)
{
	DPM_SET_FLAG(port, DPM_FLAG_MODE_ENTRY_DONE);
	DPM_CLR_FLAG(port, DPM_FLAG_ENTER_DP | DPM_FLAG_ENTER_TBT |
			DPM_FLAG_ENTER_USB4);
}

void dpm_set_mode_exit_request(int port)
{
	DPM_SET_FLAG(port, DPM_FLAG_EXIT_REQUEST);
}

static void dpm_clear_mode_exit_request(int port)
{
	DPM_CLR_FLAG(port, DPM_FLAG_EXIT_REQUEST);
}

/*
 * Returns true if the current policy requests that the EC try to enter this
 * mode on this port. If the EC is in charge of policy, the answer is always
 * yes.
 */
static bool dpm_mode_entry_requested(int port, enum typec_mode mode)
{
	/* If the AP isn't controlling policy, the EC is. */
	if (!IS_ENABLED(CONFIG_USB_PD_REQUIRE_AP_MODE_ENTRY))
		return true;

	switch (mode) {
	case TYPEC_MODE_DP:
		return !!DPM_CHK_FLAG(port, DPM_FLAG_ENTER_DP);
	case TYPEC_MODE_TBT:
		return !!DPM_CHK_FLAG(port, DPM_FLAG_ENTER_TBT);
	case TYPEC_MODE_USB4:
		return !!DPM_CHK_FLAG(port, DPM_FLAG_ENTER_USB4);
	default:
		return false;
	}
}

void dpm_vdm_acked(int port, enum tcpm_transmit_type type, int vdo_count,
		uint32_t *vdm)
{
	const uint16_t svid = PD_VDO_VID(vdm[0]);

	assert(vdo_count >= 1);

	switch (svid) {
	case USB_SID_DISPLAYPORT:
		dp_vdm_acked(port, type, vdo_count, vdm);
		break;
	case USB_VID_INTEL:
		if (IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE)) {
			intel_vdm_acked(port, type, vdo_count, vdm);
			break;
		}
	default:
		CPRINTS("C%d: Received unexpected VDM ACK for SVID %d", port,
				svid);
	}
}

void dpm_vdm_naked(int port, enum tcpm_transmit_type type, uint16_t svid,
		uint8_t vdm_cmd)
{
	switch (svid) {
	case USB_SID_DISPLAYPORT:
		dp_vdm_naked(port, type, vdm_cmd);
		break;
	case USB_VID_INTEL:
		if (IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE)) {
			intel_vdm_naked(port, type, vdm_cmd);
			break;
		}
	default:
		CPRINTS("C%d: Received unexpected VDM NAK for SVID %d", port,
				svid);
	}
}

/*
 * Requests that the PE send one VDM, whichever is next in the mode entry
 * sequence. This only happens if preconditions for mode entry are met. If
 * CONFIG_USB_PD_REQUIRE_AP_MODE_ENTRY is enabled, this function waits for the
 * AP to direct mode entry.
 */
static void dpm_attempt_mode_entry(int port)
{
	int vdo_count = 0;
	uint32_t vdm[VDO_MAX_SIZE];
	enum tcpm_transmit_type tx_type = TCPC_TX_SOP;
	bool enter_mode_requested =
		IS_ENABLED(CONFIG_USB_PD_REQUIRE_AP_MODE_ENTRY) ?  false : true;

	if (pd_get_data_role(port) != PD_ROLE_DFP) {
		if (DPM_CHK_FLAG(port, DPM_FLAG_ENTER_DP |
					DPM_FLAG_ENTER_TBT |
					DPM_FLAG_ENTER_USB4))
			DPM_CLR_FLAG(port, DPM_FLAG_ENTER_DP |
					DPM_FLAG_ENTER_TBT |
					DPM_FLAG_ENTER_USB4);
		/*
		 * TODO(b/168030639): Notify the AP that the enter mode request
		 * failed.
		 */
		return;
	}
	/*
	 * Do not try to enter mode while CPU is off.
	 * CPU transitions (e.g b/158634281) can occur during the discovery
	 * phase or during enter/exit negotiations, and the state
	 * of the modes can get out of sync, causing the attempt to
	 * enter the mode to fail prematurely.
	 */
	if (chipset_in_or_transitioning_to_state(CHIPSET_STATE_ANY_OFF))
		return;
	/*
	 * If discovery has not occurred for modes, do not attempt to switch
	 * to alt mode.
	 */
	if (pd_get_svids_discovery(port, TCPC_TX_SOP) != PD_DISC_COMPLETE ||
	    pd_get_modes_discovery(port, TCPC_TX_SOP) != PD_DISC_COMPLETE)
		return;

	if (dp_entry_is_done(port) ||
	   (IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE) &&
				tbt_entry_is_done(port)) ||
	   (IS_ENABLED(CONFIG_USB_PD_USB4) && enter_usb_entry_is_done(port))) {
		dpm_set_mode_entry_done(port);
		return;
	}

	/* Check if the device and cable support USB4. */
	if (IS_ENABLED(CONFIG_USB_PD_USB4) &&
	    enter_usb_port_partner_is_capable(port) &&
	    enter_usb_cable_is_capable(port) &&
	    dpm_mode_entry_requested(port, TYPEC_MODE_USB4)) {
		struct pd_discovery *disc_sop_prime =
			pd_get_am_discovery(port, TCPC_TX_SOP_PRIME);
		union tbt_mode_resp_cable cable_mode_resp = {
			.raw_value = pd_get_tbt_mode_vdo(port,
						TCPC_TX_SOP_PRIME) };
		/*
		 * Enter USB mode if -
		 * 1. It's a passive cable and Thunderbolt Mode SOP' VDO
		 *    active/passive bit (B25) is TBT_CABLE_PASSIVE or
		 * 2. It's a active cable with VDM version >= 2.0 and
		 *    VDO version >= 1.3 or
		 * 3. The cable has entered Thunderbolt mode.
		 */
		if ((get_usb_pd_cable_type(port) == IDH_PTYPE_PCABLE &&
		     cable_mode_resp.tbt_active_passive == TBT_CABLE_PASSIVE) ||
		    (get_usb_pd_cable_type(port) == IDH_PTYPE_ACABLE &&
		     pd_get_vdo_ver(port, TCPC_TX_SOP_PRIME) >= VDM_VER20 &&
		     disc_sop_prime->identity.product_t1.a_rev30.vdo_ver >=
							VDO_VERSION_1_3) ||
		     tbt_cable_entry_is_done(port)) {
			pd_dpm_request(port, DPM_REQUEST_ENTER_USB);
			return;
		}
	}

	/* If not, check if they support Thunderbolt alt mode. */
	if (IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE) &&
	    pd_is_mode_discovered_for_svid(port, TCPC_TX_SOP, USB_VID_INTEL) &&
			dpm_mode_entry_requested(port, TYPEC_MODE_TBT)) {
		enter_mode_requested = true;
		vdo_count = tbt_setup_next_vdm(port,
			ARRAY_SIZE(vdm), vdm, &tx_type);
	}

	/* If not, check if they support DisplayPort alt mode. */
	if (vdo_count == 0 && !DPM_CHK_FLAG(port, DPM_FLAG_MODE_ENTRY_DONE) &&
	    pd_is_mode_discovered_for_svid(port, TCPC_TX_SOP,
				USB_SID_DISPLAYPORT) &&
	    dpm_mode_entry_requested(port, TYPEC_MODE_DP)) {
		enter_mode_requested = true;
		vdo_count = dp_setup_next_vdm(port, ARRAY_SIZE(vdm), vdm);
	}

	/*
	 * If the PE didn't discover any supported (requested) alternate mode,
	 * just mark setup done and get out of here.
	 */
	if (vdo_count == 0 && !DPM_CHK_FLAG(port, DPM_FLAG_MODE_ENTRY_DONE)) {
		if (enter_mode_requested) {
			/*
			 * TODO(b/168030639): Notify the AP that mode entry
			 * failed.
			 */
			CPRINTS("C%d: No supported alt mode discovered", port);
		}
		/*
		 * If the AP did not request mode entry, it may do so in the
		 * future, but the DPM is done trying for now.
		 */
		dpm_set_mode_entry_done(port);
		return;
	}

	if (vdo_count < 0) {
		dpm_set_mode_entry_done(port);
		CPRINTS("C%d: Couldn't construct alt mode VDM", port);
		return;
	}

	/*
	 * TODO(b/155890173): Provide a host command to request that the PE send
	 * an arbitrary VDM via this mechanism.
	 */
	if (!pd_setup_vdm_request(port, tx_type, vdm, vdo_count)) {
		dpm_set_mode_entry_done(port);
		return;
	}

	pd_dpm_request(port, DPM_REQUEST_VDM);
}

static void dpm_attempt_mode_exit(int port)
{
	uint32_t vdm = 0;
	int vdo_count = 0;
	enum tcpm_transmit_type tx_type = TCPC_TX_SOP;

	if (IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE) &&
	    tbt_is_active(port)) {
		/*
		 * When the port is in USB4 mode and receives an exit request,
		 * it leaves USB4 SOP in active state.
		 * TODO(b/156749387): Support Data Reset for exiting USB4 SOP.
		 */
		CPRINTS("C%d: TBT teardown", port);
		tbt_exit_mode_request(port);
		vdo_count = tbt_setup_next_vdm(port, VDO_MAX_SIZE, &vdm,
					&tx_type);
	} else if (dp_is_active(port)) {
		CPRINTS("C%d: DP teardown", port);
		vdo_count = dp_setup_next_vdm(port, VDO_MAX_SIZE, &vdm);
	} else {
		/* Clear exit mode request */
		dpm_clear_mode_exit_request(port);
		return;
	}

	if (!pd_setup_vdm_request(port, tx_type, &vdm, vdo_count)) {
		dpm_clear_mode_exit_request(port);
		return;
	}

	pd_dpm_request(port, DPM_REQUEST_VDM);
}

void dpm_run(int port)
{
	if (DPM_CHK_FLAG(port, DPM_FLAG_EXIT_REQUEST))
		dpm_attempt_mode_exit(port);
	else if (!DPM_CHK_FLAG(port, DPM_FLAG_MODE_ENTRY_DONE))
		dpm_attempt_mode_entry(port);
}

/*
 * Source-out policy variables and APIs
 *
 * Priority for the available 3.0 A ports is given in the following order:
 * - sink partners which report requiring > 1.5 A in their Sink_Capabilities
 */

/*
 * Bitmasks of port numbers in each following category
 *
 * Note: bitmasks should be accessed atomically as all PD tasks may alter them
 */
uint32_t max_claimed;		/* Ports which have currently reserved 3 A */
uint32_t max_sink_requested;	/* Ports with a PD sink needing > 1.5 A */

/* Total ports in max_claimed, to be incremented and decremented atomically */
uint32_t total_claimed;

#define LOWEST_PORT(p) __builtin_ctz(p)  /* Undefined behavior if p == 0 */

static void allocate_3a_sink(int port)
{
	/* Increment guard total before claiming port */
	atomic_add(&total_claimed, 1);
	atomic_or(&max_claimed, BIT(port));

	typec_select_src_current_limit_rp(port, TYPEC_RP_3A0);
}

/* Process sink's first Sink_Capabilities PDO for port current consideration */
void dpm_evaluate_sink_fixed_pdo(int port, uint32_t vsafe5v_pdo)
{
	if (CONFIG_USB_PD_3A_PORTS == 0)
		return;

	/* Verify partner supplied valid vSafe5V fixed object first */
	if ((vsafe5v_pdo & PDO_TYPE_MASK) != PDO_TYPE_FIXED)
		return;

	if (PDO_FIXED_VOLTAGE(vsafe5v_pdo) != 5000)
		return;

	/* Valid PDO to process, so evaluate whether > 1.5 A is needed */
	if (PDO_FIXED_CURRENT(vsafe5v_pdo) <= 1500)
		return;

	/* If this port already has 3 A, nothing to do */
	if ((BIT(port) & max_sink_requested) && (BIT(port) & max_claimed))
		return;

	atomic_or(&max_sink_requested, BIT(port));

	if (total_claimed < CONFIG_USB_PD_3A_PORTS)
		allocate_3a_sink(port);

	/* TODO(b/141690755): Check lower priority claims to bump */
}

void dpm_remove_sink(int port)
{
	uint32_t ports_waiting;

	if (CONFIG_USB_PD_3A_PORTS == 0)
		return;

	if (!(BIT(port) & max_sink_requested))
		return;

	atomic_clear_bits(&max_sink_requested, BIT(port));

	if (!(BIT(port) & max_claimed))
		return;

	if (total_claimed > CONFIG_USB_PD_3A_PORTS) {
		/* An error has ocured, clear this port and leave */
		atomic_clear_bits(&max_claimed, BIT(port));
		atomic_sub(&total_claimed, 1);
		return;
	}

	/* Reallocate 3 A to other ports, lowest number getting priority */
	ports_waiting = max_sink_requested & ~max_claimed;
	if (ports_waiting)
		allocate_3a_sink(LOWEST_PORT(ports_waiting));

	atomic_clear_bits(&max_claimed, BIT(port));
	atomic_sub(&total_claimed, 1);
}

/*
 * Note: all ports receive the 1.5 A source offering until they are found to
 * match a criteria on the 3.0 A priority list (ex. though sink capability
 * probing), at which point they will be offered a new 3.0 A source capability.
 */
int dpm_get_source_pdo(const uint32_t **src_pdo, const int port)
{
	/* Max PDO may not exist on boards which don't offer 3 A */
#if CONFIG_USB_PD_3A_PORTS > 0
	if (max_claimed & BIT(port)) {
		*src_pdo = pd_src_pdo_max;
		return pd_src_pdo_max_cnt;
	}
#endif

	*src_pdo = pd_src_pdo;
	return pd_src_pdo_cnt;
}
