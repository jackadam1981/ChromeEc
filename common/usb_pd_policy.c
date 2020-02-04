/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "atomic.h"
#include "charge_manager.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "flash.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "mkbp_event.h"
#include "registers.h"
#include "rsa.h"
#include "sha256.h"
#include "system.h"
#include "task.h"
#include "tcpm.h"
#include "timer.h"
#include "util.h"
#include "usb_api.h"
#include "usb_common.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usbc_ppc.h"
#include "version.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#else
#define CPRINTS(format, args...)
#define CPRINTF(format, args...)
#endif

static int rw_flash_changed = 1;

#ifdef CONFIG_MKBP_EVENT
static int dp_alt_mode_entry_get_next_event(uint8_t *data)
{
	return EC_SUCCESS;
}
DECLARE_EVENT_SOURCE(EC_MKBP_EVENT_DP_ALT_MODE_ENTERED,
		     dp_alt_mode_entry_get_next_event);

void pd_notify_dp_alt_mode_entry(void)
{
	CPRINTS("Notifying AP of DP Alt Mode Entry...");
	mkbp_send_event(EC_MKBP_EVENT_DP_ALT_MODE_ENTERED);
}
#endif /* CONFIG_MKBP_EVENT */

int pd_check_requested_voltage(uint32_t rdo, const int port)
{
	int max_ma = rdo & 0x3FF;
	int op_ma = (rdo >> 10) & 0x3FF;
	int idx = RDO_POS(rdo);
	uint32_t pdo;
	uint32_t pdo_ma;
#if defined(CONFIG_USB_PD_DYNAMIC_SRC_CAP) || \
		defined(CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT)
	const uint32_t *src_pdo;
	const int pdo_cnt = charge_manager_get_source_pdo(&src_pdo, port);
#else
	const uint32_t *src_pdo = pd_src_pdo;
	const int pdo_cnt = pd_src_pdo_cnt;
#endif

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
		 ((pdo >> 10) & 0x3ff) * 50, (pdo & 0x3ff) * 10,
		 op_ma * 10, max_ma * 10);

	/* Accept the requested voltage */
	return EC_SUCCESS;
}

__attribute__((weak)) int pd_board_check_request(uint32_t rdo, int pdo_cnt)
{
	int idx = RDO_POS(rdo);

	/* Check for invalid index */
	return (!idx || idx > pdo_cnt) ?
		EC_ERROR_INVAL : EC_SUCCESS;
}

#ifdef CONFIG_USB_PD_DUAL_ROLE
/* Last received source cap */
static uint32_t pd_src_caps[CONFIG_USB_PD_PORT_MAX_COUNT][PDO_MAX_OBJECTS];
static uint8_t pd_src_cap_cnt[CONFIG_USB_PD_PORT_MAX_COUNT];

/* Cap on the max voltage requested as a sink (in millivolts) */
static unsigned max_request_mv = PD_MAX_VOLTAGE_MV; /* no cap */

const uint32_t * const pd_get_src_caps(int port)
{
	ASSERT(port < CONFIG_USB_PD_PORT_MAX_COUNT);

	return pd_src_caps[port];
}

uint8_t pd_get_src_cap_cnt(int port)
{
	ASSERT(port < CONFIG_USB_PD_PORT_MAX_COUNT);

	return pd_src_cap_cnt[port];
}

void pd_process_source_cap(int port, int cnt, uint32_t *src_caps)
{
#ifdef CONFIG_CHARGE_MANAGER
	uint32_t ma, mv, pdo;
#endif
	int i;

	pd_src_cap_cnt[port] = cnt;
	for (i = 0; i < cnt; i++)
		pd_src_caps[port][i] = *src_caps++;

#ifdef CONFIG_CHARGE_MANAGER
	/* Get max power info that we could request */
	pd_find_pdo_index(pd_get_src_cap_cnt(port), pd_get_src_caps(port),
						PD_MAX_VOLTAGE_MV, &pdo);
	pd_extract_pdo_power(pdo, &ma, &mv);

	/* Set max. limit, but apply 500mA ceiling */
	charge_manager_set_ceil(port, CEIL_REQUESTOR_PD, PD_MIN_MA);
	pd_set_input_current_limit(port, ma, mv);
#endif
}

void pd_set_max_voltage(unsigned mv)
{
	max_request_mv = mv;
}

unsigned pd_get_max_voltage(void)
{
	return max_request_mv;
}

int pd_charge_from_device(uint16_t vid, uint16_t pid)
{
	/* TODO: rewrite into table if we get more of these */
	/*
	 * White-list Apple charge-through accessory since it doesn't set
	 * unconstrained bit, but we still need to charge from it when
	 * we are a sink.
	 */
	return (vid == USB_VID_APPLE && (pid == 0x1012 || pid == 0x1013));
}
#endif /* CONFIG_USB_PD_DUAL_ROLE */

static struct pd_cable cable[CONFIG_USB_PD_PORT_MAX_COUNT];

bool is_transmit_msg_sop_prime(int port)
{
	return (IS_ENABLED(CONFIG_USB_PD_DECODE_SOP) &&
		(cable[port].flags & CABLE_FLAGS_SOP_PRIME_ENABLE));
}

bool is_transmit_msg_sop_prime_prime(int port)
{
	return (IS_ENABLED(CONFIG_USB_PD_DECODE_SOP) &&
		(cable[port].flags & CABLE_FLAGS_SOP_PRIME_PRIME_ENABLE));
}

bool consume_sop_prime_repeat_msg(int port, uint8_t msg_id)
{

	if (cable[port].last_sop_p_msg_id != msg_id) {
		cable[port].last_sop_p_msg_id = msg_id;
		return false;
	}
	CPRINTF("C%d SOP Prime repeat msg_id %d\n", port, msg_id);
	return true;
}

bool consume_sop_prime_prime_repeat_msg(int port, uint8_t msg_id)
{

	if (cable[port].last_sop_p_p_msg_id != msg_id) {
		cable[port].last_sop_p_p_msg_id = msg_id;
		return false;
	}
	CPRINTF("C%d SOP Prime Prime repeat msg_id %d\n", port, msg_id);
	return true;
}

void disable_transmit_sop_prime(int port)
{
	if (IS_ENABLED(CONFIG_USB_PD_DECODE_SOP))
		cable[port].flags &= ~CABLE_FLAGS_SOP_PRIME_ENABLE;
}

void disable_transmit_sop_prime_prime(int port)
{
	if (IS_ENABLED(CONFIG_USB_PD_DECODE_SOP))
		cable[port].flags &= ~CABLE_FLAGS_SOP_PRIME_PRIME_ENABLE;
}

enum pd_msg_type pd_msg_tx_type(int port, enum pd_data_role data_role,
				uint32_t pd_flags)
{
	/*
	 * Ref: USB PD 3.0 sec 2.5.4: When an Explicit Contract is in place the
	 * VCONN Source (either the DFP or the UFP) can communicate with the
	 * Cable Plug(s) using SOP’/SOP’’ Packets
	 *
	 * Ref: USB PD 2.0 sec 2.4.4: When an Explicit Contract is in place the
	 * DFP (either the Source or the Sink) can communicate with the
	 * Cable Plug(s) using SOP’/SOP” Packets.
	 * Sec 3.6.11 : Before communicating with a Cable Plug a Port Should
	 * ensure that it is the Vconn Source
	 */
	if (pd_flags & PD_FLAGS_VCONN_ON && (IS_ENABLED(CONFIG_USB_PD_REV30) ||
		data_role == PD_ROLE_DFP)) {
		if (is_transmit_msg_sop_prime(port))
			return PD_MSG_SOP_PRIME;
		if (is_transmit_msg_sop_prime_prime(port))
			return PD_MSG_SOP_PRIME_PRIME;
	}

	if (is_transmit_msg_sop_prime(port)) {
		/*
		 * Clear the CABLE_FLAGS_SOP_PRIME_ENABLE flag if the port is
		 * unable to communicate with the cable plug.
		 */
		disable_transmit_sop_prime(port);
	} else if (is_transmit_msg_sop_prime_prime(port)) {
		/*
		 * Clear the CABLE_FLAGS_SOP_PRIME_PRIME_ENABLE flag if the port
		 * is unable to communicate with the cable plug.
		 */
		disable_transmit_sop_prime_prime(port);
	}

	return PD_MSG_SOP;
}

void reset_pd_cable(int port)
{
	if (IS_ENABLED(CONFIG_USB_PD_DECODE_SOP)) {
		memset(&cable[port], 0, sizeof(cable[port]));
		cable[port].last_sop_p_msg_id = INVALID_MSG_ID_COUNTER;
		cable[port].last_sop_p_p_msg_id = INVALID_MSG_ID_COUNTER;
	}
}

enum idh_ptype get_usb_pd_cable_type(int port)
{
	return cable[port].type;
}

union tbt_mode_resp_cable get_cable_tbt_vdo(int port)
{
	/*
	 * Return Discover mode SOP prime response for Thunderbolt-compatible
	 * mode SVDO.
	 */
	return cable[port].cable_mode_resp;
}

union tbt_mode_resp_device get_dev_tbt_vdo(int port)
{
	/*
	 * Return Discover mode SOP response for Thunderbolt-compatible
	 * mode SVDO.
	 */
	return cable[port].dev_mode_resp;
}

enum tbt_compat_cable_speed get_tbt_cable_speed(int port)
{
	/* tbt_cable_speed is zero when uninitialized */
	return cable[port].cable_mode_resp.tbt_cable_speed;
}

enum tbt_compat_rounded_support get_tbt_rounded_support(int port)
{
	/* tbt_rounded_support is zero when uninitialized */
	return cable[port].cable_mode_resp.tbt_rounded;
}

static enum usb_rev30_ss get_usb4_cable_speed(int port)
{
	if ((cable[port].rev == PD_REV30) &&
	    (get_usb_pd_cable_type(port) == IDH_PTYPE_PCABLE) &&
	   ((cable[port].attr.p_rev30.ss != USB_R30_SS_U32_U40_GEN2) ||
	    !IS_ENABLED(CONFIG_USB_PD_TBT_GEN3_CAPABLE))) {
		return cable[port].attr.p_rev30.ss;
	}

	/*
	 * Converting Thunderolt-Compatible cable speed to equivalent USB4 cable
	 * speed.
	 */
	return cable[port].cable_mode_resp.tbt_cable_speed == TBT_SS_TBT_GEN3 ?
	       USB_R30_SS_U40_GEN3 : USB_R30_SS_U32_U40_GEN2;
}

uint32_t get_enter_usb_msg_payload(int port)
{
	/*
	 * Ref: USB Power Delivery Specification Revision 3.0, Version 2.0
	 * Table 6-47 Enter_USB Data Object
	 */
	union enter_usb_data_obj eudo;

	if (!IS_ENABLED(CONFIG_USB_PD_USB4))
		return 0;

	eudo.mode = USB_PD_40;
	eudo.usb4_drd_cap = IS_ENABLED(CONFIG_USB_PD_USB4);
	eudo.usb3_drd_cap = IS_ENABLED(CONFIG_USB_PD_USB32);
	eudo.cable_speed = get_usb4_cable_speed(port);

	if ((cable[port].rev == PD_REV30) &&
	    (get_usb_pd_cable_type(port) == IDH_PTYPE_ACABLE)) {
		eudo.cable_type = (cable[port].attr2.a2_rev30.active_elem ==
			ACTIVE_RETIMER) ? CABLE_TYPE_ACTIVE_RETIMER :
			CABLE_TYPE_ACTIVE_REDRIVER;
	/* TODO: Add eudo.cable_type for Revisiosn 2 active cables */
	} else {
		eudo.cable_type = CABLE_TYPE_PASSIVE;
	}

	switch (cable[port].attr.p_rev20.vbus_cur) {
	case USB_VBUS_CUR_3A:
		eudo.cable_current = USB4_CABLE_CURRENT_3A;
		break;
	case USB_VBUS_CUR_5A:
		eudo.cable_current = USB4_CABLE_CURRENT_5A;
		break;
	default:
		eudo.cable_current = USB4_CABLE_CURRENT_INVALID;
		break;
	}
	eudo.pcie_supported = IS_ENABLED(CONFIG_USB_PD_PCIE_TUNNELING);
	eudo.dp_supported = IS_ENABLED(CONFIG_USB_PD_ALT_MODE_DFP);
	eudo.tbt_supported = IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE);
	eudo.host_present = 1;

	return eudo.raw_value;
}

bool should_enter_usb4_mode(int port)
{
	return IS_ENABLED(CONFIG_USB_PD_USB4) &&
		cable[port].flags & CABLE_FLAGS_ENTER_USB_MODE;
}

void disable_enter_usb4_mode(int port)
{
	if (IS_ENABLED(CONFIG_USB_PD_USB4))
		cable[port].flags &= ~CABLE_FLAGS_ENTER_USB_MODE;
}

#ifdef CONFIG_USB_PD_ALT_MODE

#ifdef CONFIG_USB_PD_ALT_MODE_DFP

static struct pd_policy pe[CONFIG_USB_PD_PORT_MAX_COUNT];

void pd_dfp_pe_init(int port)
{
	memset(&pe[port], 0, sizeof(struct pd_policy));
}

/* Note: Enter mode flag is not needed by TCPMv1 */
void pd_set_dfp_enter_mode_flag(int port, bool set)
{
}

struct pd_cable *pd_get_cable_attributes(int port)
{
	return &cable[port];
}

struct pd_policy *pd_get_am_policy(int port)
{
	return &pe[port];
}

__overridable enum tbt_compat_cable_speed board_get_max_tbt_speed(int port)
{
	return cable[port].cable_mode_resp.tbt_cable_speed;
}

#endif /* CONFIG_USB_PD_ALT_MODE_DFP */

int pd_svdm(int port, int cnt, uint32_t *payload, uint32_t **rpayload,
		uint16_t head)
{
	int cmd = PD_VDO_CMD(payload[0]);
	int cmd_type = PD_VDO_CMDT(payload[0]);
	int (*func)(int port, uint32_t *payload) = NULL;

	int rsize = 1; /* VDM header at a minimum */

	payload[0] &= ~VDO_CMDT_MASK;
	*rpayload = payload;

	if (cmd_type == CMDT_INIT) {
		switch (cmd) {
		case CMD_DISCOVER_IDENT:
			func = svdm_rsp.identity;
			break;
		case CMD_DISCOVER_SVID:
			func = svdm_rsp.svids;
			break;
		case CMD_DISCOVER_MODES:
			func = svdm_rsp.modes;
			break;
		case CMD_ENTER_MODE:
			func = svdm_rsp.enter_mode;
			break;
		case CMD_DP_STATUS:
			if (svdm_rsp.amode)
				func = svdm_rsp.amode->status;
			break;
		case CMD_DP_CONFIG:
			if (svdm_rsp.amode)
				func = svdm_rsp.amode->config;
			break;
		case CMD_EXIT_MODE:
			func = svdm_rsp.exit_mode;
			break;
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
		case CMD_ATTENTION:
			/*
			 * attention is only SVDM with no response
			 * (just goodCRC) return zero here.
			 */
			dfp_consume_attention(port, payload);
			return 0;
#endif
		default:
			CPRINTF("ERR:CMD:%d\n", cmd);
			rsize = 0;
		}
		if (func)
			rsize = func(port, payload);
		else /* not supported : NACK it */
			rsize = 0;
		if (rsize >= 1)
			payload[0] |= VDO_CMDT(CMDT_RSP_ACK);
		else if (!rsize) {
			payload[0] |= VDO_CMDT(CMDT_RSP_NAK);
			rsize = 1;
		} else {
			payload[0] |= VDO_CMDT(CMDT_RSP_BUSY);
			rsize = 1;
		}
		payload[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port));
	} else if (cmd_type == CMDT_RSP_ACK) {
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
		struct svdm_amode_data *modep;

		modep = pd_get_amode_data(port, PD_VDO_VID(payload[0]));
#endif
		switch (cmd) {
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
		case CMD_DISCOVER_IDENT:
			rsize = dfp_handle_acked_discover_ident(port, cnt,
								payload, head);
			break;
		case CMD_DISCOVER_SVID:
			rsize = dfp_handle_acked_discover_svid(port, cnt,
								payload);
			break;
		case CMD_DISCOVER_MODES:
			rsize = dfp_handle_acked_discover_modes(port, cnt,
								payload);
			break;
		case CMD_ENTER_MODE:
			rsize = dfp_handle_acked_enter_mode(port, cnt, payload, modep);
			break;
		case CMD_DP_STATUS:
			/* DP status response & UFP's DP attention have same
			   payload */
			dfp_consume_attention(port, payload);
			if (modep && modep->opos)
				rsize = modep->fx->config(port, payload);
			else
				rsize = 0;
			break;
		case CMD_DP_CONFIG:
			if (modep && modep->opos && modep->fx->post_config)
				modep->fx->post_config(port);
			/* no response after DFPs ack */
			rsize = 0;
			break;
		case CMD_EXIT_MODE:
			/* no response after DFPs ack */
			rsize = 0;
			break;
#endif
		case CMD_ATTENTION:
			/* no response after DFPs ack */
			rsize = 0;
			break;
		default:
			CPRINTF("ERR:CMD:%d\n", cmd);
			rsize = 0;
		}

		payload[0] |= VDO_CMDT(CMDT_INIT);
		payload[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port));
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	} else if (cmd_type == CMDT_RSP_BUSY) {
		switch (cmd) {
		case CMD_DISCOVER_IDENT:
		case CMD_DISCOVER_SVID:
		case CMD_DISCOVER_MODES:
			/* resend if its discovery */
			rsize = 1;
			break;
		case CMD_ENTER_MODE:
			/* Error */
			CPRINTF("ERR:ENTBUSY\n");
			rsize = 0;
			break;
		case CMD_EXIT_MODE:
			rsize = 0;
			break;
		default:
			rsize = 0;
		}
	} else if (cmd_type == CMDT_RSP_NAK) {
		rsize = 0;
		/* Passive cable Nacked for Discover SVID */
		if (cmd == CMD_DISCOVER_SVID)
			rsize = dfp_handle_nacked_discover_svid(port, cnt,
								payload);
#endif /* CONFIG_USB_PD_ALT_MODE_DFP */
	} else {
		CPRINTF("ERR:CMDT:%d\n", cmd);
		/* do not answer */
		rsize = 0;
	}
	return rsize;
}

#else

int pd_svdm(int port, int cnt, uint32_t *payload, uint32_t **rpayload,
		uint16_t head)
{
	return 0;
}

#endif /* CONFIG_USB_PD_ALT_MODE */

static void pd_usb_billboard_deferred(void)
{
#if defined(CONFIG_USB_PD_ALT_MODE) && !defined(CONFIG_USB_PD_ALT_MODE_DFP) \
	&& !defined(CONFIG_USB_PD_SIMPLE_DFP) && defined(CONFIG_USB_BOS)

	/*
	 * TODO(tbroch)
	 * 1. Will we have multiple type-C port UFPs
	 * 2. Will there be other modes applicable to DFPs besides DP
	 */
	if (!pd_alt_mode(0, USB_SID_DISPLAYPORT))
		usb_connect();

#endif
}
DECLARE_DEFERRED(pd_usb_billboard_deferred);

#define FW_RW_END (CONFIG_EC_WRITABLE_STORAGE_OFF + \
		   CONFIG_RW_STORAGE_OFF + CONFIG_RW_SIZE)

uint8_t *flash_hash_rw(void)
{
	static struct sha256_ctx ctx;

	/* re-calculate RW hash when changed as its time consuming */
	if (rw_flash_changed) {
		rw_flash_changed = 0;
		SHA256_init(&ctx);
		SHA256_update(&ctx, (void *)CONFIG_PROGRAM_MEMORY_BASE +
			      CONFIG_RW_MEM_OFF,
			      CONFIG_RW_SIZE - RSANUMBYTES);
		return SHA256_final(&ctx);
	} else {
		return ctx.buf;
	}
}

void pd_get_info(uint32_t *info_data)
{
	void *rw_hash = flash_hash_rw();

	/* copy first 20 bytes of RW hash */
	memcpy(info_data, rw_hash, 5 * sizeof(uint32_t));
	/* copy other info into data msg */
#if defined(CONFIG_USB_PD_HW_DEV_ID_BOARD_MAJOR) && \
	defined(CONFIG_USB_PD_HW_DEV_ID_BOARD_MINOR)
	info_data[5] = VDO_INFO(CONFIG_USB_PD_HW_DEV_ID_BOARD_MAJOR,
				CONFIG_USB_PD_HW_DEV_ID_BOARD_MINOR,
				ver_get_num_commits(system_get_image_copy()),
				(system_get_image_copy() != SYSTEM_IMAGE_RO));
#else
	info_data[5] = 0;
#endif
}

int pd_custom_flash_vdm(int port, int cnt, uint32_t *payload)
{
	static int flash_offset;
	int rsize = 1; /* default is just VDM header returned */

	switch (PD_VDO_CMD(payload[0])) {
	case VDO_CMD_VERSION:
		memcpy(payload + 1, &current_image_data.version, 24);
		rsize = 7;
		break;
	case VDO_CMD_REBOOT:
		/* ensure the power supply is in a safe state */
		pd_power_supply_reset(0);
		system_reset(0);
		break;
	case VDO_CMD_READ_INFO:
		/* copy info into response */
		pd_get_info(payload + 1);
		rsize = 7;
		break;
	case VDO_CMD_FLASH_ERASE:
		/* do not kill the code under our feet */
		if (system_get_image_copy() != SYSTEM_IMAGE_RO)
			break;
		pd_log_event(PD_EVENT_ACC_RW_ERASE, 0, 0, NULL);
		flash_offset = CONFIG_EC_WRITABLE_STORAGE_OFF +
			       CONFIG_RW_STORAGE_OFF;
		flash_physical_erase(CONFIG_EC_WRITABLE_STORAGE_OFF +
				     CONFIG_RW_STORAGE_OFF, CONFIG_RW_SIZE);
		rw_flash_changed = 1;
		break;
	case VDO_CMD_FLASH_WRITE:
		/* do not kill the code under our feet */
		if ((system_get_image_copy() != SYSTEM_IMAGE_RO) ||
		    (flash_offset < CONFIG_EC_WRITABLE_STORAGE_OFF +
				    CONFIG_RW_STORAGE_OFF))
			break;
		flash_physical_write(flash_offset, 4*(cnt - 1),
				     (const char *)(payload+1));
		flash_offset += 4*(cnt - 1);
		rw_flash_changed = 1;
		break;
	case VDO_CMD_ERASE_SIG:
		/* this is not touching the code area */
		{
			uint32_t zero = 0;
			int offset;
			/* zeroes the area containing the RSA signature */
			for (offset = FW_RW_END - RSANUMBYTES;
			     offset < FW_RW_END; offset += 4)
				flash_physical_write(offset, 4,
						     (const char *)&zero);
		}
		break;
	default:
		/* Unknown : do not answer */
		return 0;
	}
	return rsize;
}

#ifdef CONFIG_USB_PD_DISCHARGE
void pd_set_vbus_discharge(int port, int enable)
{
	static struct mutex discharge_lock[CONFIG_USB_PD_PORT_MAX_COUNT];

	mutex_lock(&discharge_lock[port]);
	enable &= !board_vbus_source_enabled(port);
#ifdef CONFIG_USB_PD_DISCHARGE_GPIO
	if (!port)
		gpio_set_level(GPIO_USB_C0_DISCHARGE, enable);
#if CONFIG_USB_PD_PORT_MAX_COUNT > 1
	else
		gpio_set_level(GPIO_USB_C1_DISCHARGE, enable);
#endif /* CONFIG_USB_PD_PORT_MAX_COUNT */
#elif defined(CONFIG_USB_PD_DISCHARGE_TCPC)
	tcpc_discharge_vbus(port, enable);
#elif defined(CONFIG_USB_PD_DISCHARGE_PPC)
	ppc_discharge_vbus(port, enable);
#else
#error "PD discharge implementation not defined"
#endif
	mutex_unlock(&discharge_lock[port]);
}
#endif /* CONFIG_USB_PD_DISCHARGE */
