/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "chip/stm32/ucpd-stm32gx.h"
#include "driver/tcpm/tcpci.h"
#include "driver/mp4245.h"
#include "task.h"
#include "timer.h"
#include "usb_common.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usbc_ppc.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

#define PDO_FIXED_FLAGS_EXT (PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP |\
			     PDO_FIXED_COMM_CAP | PDO_FIXED_UNCONSTRAINED)

#define PDO_FIXED_FLAGS (PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP |\
			 PDO_FIXED_COMM_CAP | PDO_FIXED_UNCONSTRAINED)

/* Voltage indexes for the PDOs */
enum volt_idx {
	PDO_IDX_5V   = 0,
#ifdef BOARD_ALLOW_HIGH_VBUS
	PDO_IDX_9V   = 1,
	PDO_IDX_15V  = 2,
	PDO_IDX_20V  = 3,
#endif
	/* TODO: add PPS support */
	PDO_IDX_COUNT
};

const uint32_t pd_src_host_pdo[] = {
	[PDO_IDX_5V]  = PDO_FIXED(5000,   3000, PDO_FIXED_FLAGS),
#ifdef BOARD_ALLOW_HIGH_VBUS
	[PDO_IDX_9V]  = PDO_FIXED(9000,   3000, PDO_FIXED_FLAGS),
	[PDO_IDX_15V]  = PDO_FIXED(15000, 3000, PDO_FIXED_FLAGS),
	[PDO_IDX_20V]  = PDO_FIXED(20000, 3000, PDO_FIXED_FLAGS),
#endif
};
BUILD_ASSERT(ARRAY_SIZE(pd_src_host_pdo) == PDO_IDX_COUNT);

const uint32_t pd_snk_pdo[] = {
	PDO_FIXED(5000, 1500, PDO_FIXED_FLAGS),
};
const int pd_snk_pdo_cnt = ARRAY_SIZE(pd_snk_pdo);

int charge_manager_get_source_pdo(const uint32_t **src_pdo, const int port)
{
	int pdo_cnt = 0;

	*src_pdo =  pd_src_host_pdo;
	pdo_cnt = ARRAY_SIZE(pd_src_host_pdo);

	return pdo_cnt;
}

int pd_check_vconn_swap(int port)
{
	/*TODO: Dock is the Vconn source */
	return 1;
}

void pd_power_supply_reset(int port)
{
	int prev_en;

	if (port < 0 || port >= CONFIG_USB_PD_PORT_MAX_COUNT)
		return;

	prev_en = ppc_is_sourcing_vbus(port);

	/* Disable VBUS. */
	ppc_vbus_source_enable(port, 0);
	/* Enable discharge if we were previously sourcing 5V */
	if (prev_en) {
		pd_set_vbus_discharge(port, 1);
	}

	if (port == USB_PD_PORT_HOST) {
		/* Turn off voltage output from buck-boost */
		mp4245_votlage_out_enable(0);
		/* Reset VBUS voltage to default value (fixed 5V SRC_CAP) */
		pd_transition_voltage(1);
	}
}

int pd_set_power_supply_ready(int port)
{
	int rv;

	if (port == USB_PD_PORT_HOST) {
		/* Ensure buck-boost is enabled and Vout is on */
		mp4245_votlage_out_enable(1);
		msleep(MP4245_VOUT_5V_DELAY_MS);
	}


	/*
	 * Default operation of buck-boost is 5v/3.6A.
	 * Turn on the PPC Provide Vbus.
	 */
	rv = ppc_vbus_source_enable(port, 1);
	if (rv)
		return rv;

	return EC_SUCCESS;
}

void pd_transition_voltage(int idx)
{
	int port = TASK_ID_TO_PD_PORT(task_get_current());

	if (port == USB_PD_PORT_HOST) {
		int mv;
		int ma;
		int vbus_thresh;
		int i;

	/*
	 * Set the VBUS output voltage and current limit to the values specified
	 * by the PDO requested by sink. Note that USB PD uses idx = 1 for 1st
	 * PDO of SRC_CAP which must aways be 5V fixed supply.
	 */
		pd_extract_pdo_power(pd_src_host_pdo[idx - 1], &ma, &mv);

		/* Set VBUS level to value specified in the requested PDO */
		mp4245_set_voltage_out(mv);
		/* Wait for vbus to be with 95% of its target value */
		vbus_thresh = mv - (mv >> 4);

		for (i = 0; i < 20; i++) {
			int rv;

			rv =  mp3245_get_vbus(&mv, &ma);
			if ((rv == EC_SUCCESS) && (mv >= vbus_thresh))
				return;
			msleep(2);
		}
	}
}

int pd_snk_is_vbus_provided(int port)
{
	return ppc_is_vbus_present(port);
}

int board_vbus_source_enabled(int port)
{
	return ppc_is_sourcing_vbus(port);
}

void pd_set_input_current_limit(int port, uint32_t max_ma,
				uint32_t supply_voltage)
{

}

int pd_check_data_swap(int port,
	enum pd_data_role data_role)
{
	int swap = 0;

	if (port == 0)
		swap = (data_role == PD_ROLE_DFP);
	else if (port == 1)
		swap = (data_role == PD_ROLE_UFP);

	return swap;
}

int pd_check_power_swap(int port)
{

	if (pd_get_power_role(port) == PD_ROLE_SINK)
		return 1;

	return 0;
}

static int vdm_is_dp_enabled(int port)
{
	mux_state_t mux_state = usb_mux_get(port);

	return !!(mux_state & USB_PD_MUX_DP_ENABLED);
}

/* ----------------- Vendor Defined Messages ------------------ */
#ifndef TCPM_V2_ALT_MODE
/* Holds valid object position (opos) for entered mode */
static int alt_mode[PD_AMODE_COUNT];
#endif

const uint32_t vdo_idh = VDO_IDH(0, /* data caps as USB host */
				 1, /* data caps as USB device */
				 IDH_PTYPE_AMA, /* Alternate mode */
				 1, /* supports alt modes */
				 USB_VID_GOOGLE);

const uint32_t vdo_product = VDO_PRODUCT(CONFIG_USB_PID, CONFIG_USB_BCD_DEV);

const uint32_t vdo_ama = VDO_AMA(CONFIG_USB_PD_IDENTITY_HW_VERS,
				 CONFIG_USB_PD_IDENTITY_SW_VERS,
				 0, 0, 0, 0, /* SS[TR][12] */
				 0, /* Vconn power */
				 0, /* Vconn power required */
				 1, /* Vbus power required */
				 AMA_USBSS_BBONLY /* USB SS support */);

static int svdm_response_identity(int port, uint32_t *payload)
{
	payload[VDO_I(IDH)] = vdo_idh;
	/* TODO(tbroch): Do we plan to obtain TID (test ID) for hoho */
	payload[VDO_I(CSTAT)] = VDO_CSTAT(0);
	payload[VDO_I(PRODUCT)] = vdo_product;
	payload[VDO_I(AMA)] = vdo_ama;
	return VDO_I(AMA) + 1;
}

static int svdm_response_svids(int port, uint32_t *payload)
{
	payload[1] = USB_SID_DISPLAYPORT << 16;
	/* number of data objects VDO header + 1 SVID for DP */
	return 2;
}

#define OPOS_DP 1
#define OPOS_GFU 1

const uint32_t vdo_dp_modes[1] =  {
	VDO_MODE_DP(MODE_DP_PIN_C, /* UFP pin cfg supported : none */
		    0, /* DFP pin cfg supported */
		    1,		   /* no usb2.0	signalling in AMode */
		    CABLE_RECEPTACLE,	   /* its a receptacle */
		    MODE_DP_V13,   /* DPv1.3 Support, no Gen2 */
		    MODE_DP_SNK)   /* Its a sink only */
};

const uint32_t vdo_goog_modes[1] =  {
	VDO_MODE_GOOGLE(MODE_GOOGLE_FU)
};

static int svdm_response_modes(int port, uint32_t *payload)
{
	if (PD_VDO_VID(payload[0]) == USB_SID_DISPLAYPORT) {
		memcpy(payload + 1, vdo_dp_modes, sizeof(vdo_dp_modes));
		return ARRAY_SIZE(vdo_dp_modes) + 1;
	} else if (PD_VDO_VID(payload[0]) == USB_VID_GOOGLE) {
		memcpy(payload + 1, vdo_goog_modes, sizeof(vdo_goog_modes));
		return ARRAY_SIZE(vdo_goog_modes) + 1;
	} else {
		return 0; /* nak */
	}
}

static int amode_dp_status(int port, uint32_t *payload)
{
	int opos = PD_VDO_OPOS(payload[0]);
	int hpd = gpio_get_level(GPIO_DP_HPD);
	if (opos != OPOS_DP)
		return 0; /* nak */

	payload[1] = VDO_DP_STATUS(0,                /* IRQ_HPD */
				   (hpd == 1),       /* HPD_HI|LOW */
				   0,		     /* request exit DP */
				   0,		     /* request exit USB */
				   0,		     /* MF pref */
				   vdm_is_dp_enabled(port),
				   0,		     /* power low */
				   0x2);
	return 2;
}

static int amode_dp_config(int port, uint32_t *payload)
{
	/* Called on via alternate mode entry */
	return 1;
}

static void svdm_configure_demux(int port, int enable)
{
	mux_state_t demux = usb_mux_get(port);

	if (enable) {
		demux |= USB_PD_MUX_DP_ENABLED;
		demux &= ~USB_PD_MUX_USB_ENABLED;
	} else {
		demux &= ~USB_PD_MUX_DP_ENABLED;
		demux |= USB_PD_MUX_USB_ENABLED;
	}

	/*
	 * Update demux setting for DP mode.
	 * TODO(b/): Right now USB enable bit is being ignored. But, 4 lane DP
	 * will require disabling USB mode and then that would be reenalbed when
	 * the ALT-DP mode is exited. The switch mode can always be set as
	 * USB_SWITCH_CONNECT because switch connect/disconnect is handled at
	 * type-c layer.
	 *
	 */

	usb_mux_set(port, demux, USB_SWITCH_CONNECT, pd_get_polarity(port));
}

static int svdm_enter_mode(int port, uint32_t *payload)
{
#ifdef TCPM_V2_ALT_MODE
	struct svdm_amode_data *modep;
#endif
	int rv = 0; /* will generate a NAK */

	/* SID & mode request is valid */
	if ((PD_VDO_VID(payload[0]) == USB_SID_DISPLAYPORT) &&
	    (PD_VDO_OPOS(payload[0]) == OPOS_DP)) {
#ifdef TCPM_V2_ALT_MODE
		modep = pd_get_amode_data(port, 0, USB_SID_DISPLAYPORT);
		if (modep) {
			CPRINTS("alt-dp: enter: modep = %p, opos = %d",
				modep, modep->opos);
			modep->opos = OPOS_DP;
		} else {
			CPRINTS("alt-dp: enter: modep is null");
		}
#else
		alt_mode[PD_AMODE_DISPLAYPORT] = OPOS_DP;
#endif
		rv = 1;
		pd_log_event(PD_EVENT_VIDEO_DP_MODE, 0, 1, NULL);
		/* Configure demux to enable DP */
		svdm_configure_demux(port, 1);
		/* Entering ALT-DP mode, enable DP connection in demux */
	} else if ((PD_VDO_VID(payload[0]) == USB_VID_GOOGLE) &&
		   (PD_VDO_OPOS(payload[0]) == OPOS_GFU)) {
#ifndef TCPM_V2_ALT_MODE
		alt_mode[PD_AMODE_GOOGLE] = OPOS_GFU;
#endif
		rv = 1;
	}

	/* if (rv) */
		/*
		 * If we failed initial mode entry we'll have enumerated the USB
		 * Billboard class.  If so we should disconnect.
		 */
		/* TODO(b/): When we have usb support, put this back in? */
		/* usb_disconnect(); */

	CPRINTS("svdm_enter[%d]: svid = %x, ret = %d", port,
		PD_VDO_VID(payload[0]), rv);

	return rv;
}

#ifndef TCPM_V2_ALT_MODE
int pd_ufp_alt_mode(int port, enum tcpm_transmit_type type, uint16_t svid)
{
	if (svid == USB_SID_DISPLAYPORT)
		return alt_mode[PD_AMODE_DISPLAYPORT];
	else if (svid == USB_VID_GOOGLE)
		return alt_mode[PD_AMODE_GOOGLE];
	return 0;
}
#endif

static int svdm_exit_mode(int port, uint32_t *payload)
{
#ifdef TCPM_V2_ALT_MODE
	struct svdm_amode_data *modep;
#endif

	if (PD_VDO_VID(payload[0]) == USB_SID_DISPLAYPORT) {
#ifdef TCPM_V2_ALT_MODE
		modep = pd_get_amode_data(port, 0, USB_SID_DISPLAYPORT);
		if (modep)
			modep->opos = OPOS_DP;
		else
			CPRINTS("alt-dp: modep is NULL");
#else
		alt_mode[PD_AMODE_DISPLAYPORT] = 0;
#endif
		/* Configure demux to enable DP */
		svdm_configure_demux(port, 0);
		pd_log_event(PD_EVENT_VIDEO_DP_MODE, 0, 0, NULL);
	} else if (PD_VDO_VID(payload[0]) == USB_VID_GOOGLE) {
#ifndef TCPM_V2_ALT_MODE
		alt_mode[PD_AMODE_GOOGLE] = 0;
#endif
	} else {
		CPRINTF("Unknown exit mode req:0x%08x\n", payload[0]);
	}

	return 1; /* Must return ACK */
}

static struct amode_fx dp_fx = {
	.status = &amode_dp_status,
	.config = &amode_dp_config,
};

const struct svdm_response svdm_rsp = {
	.identity = &svdm_response_identity,
	.svids = &svdm_response_svids,
	.modes = &svdm_response_modes,
	.enter_mode = &svdm_enter_mode,
	.amode = &dp_fx,
	.exit_mode = &svdm_exit_mode,
};

int pd_custom_vdm(int port, int cnt, uint32_t *payload,
		  uint32_t **rpayload)
{
	/* We don't support, so ignore this message */
	return 0;
}
