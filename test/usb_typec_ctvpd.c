/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test USB Type-C VPD and CTVPD module.
 */
#include "common.h"
#include "crc.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "usb_pd.h"
#include "usb_sm.h"
#include "usb_tc_sm.h"
#include "util.h"
#include "usb_pd_tcpm.h"
#include "usb_pd_test_util.h"
#include "vpd_api.h"

#define PORT0	0
#define CYCLE_TIME (10 * MSEC)

enum cc_type {CC1, CC2};
enum vbus_type {VBUS_0 = 0, VBUS_5 = 5000};
enum vconn_type {VCONN_0 = 0, VCONN_3 = 3000, VCONN_5 = 5000};
enum snk_con_voltage_type {SRC_CON_DEF, SRC_CON_1_5, SRC_CON_3_0};

struct pd_port_t {
	int host_mode;
	int has_vbus;
	int msg_tx_id;
	int msg_rx_id;
	int polarity;
	int partner_role; /* -1 for none */
	int partner_polarity;
	int rev;
} pd_port[CONFIG_USB_PD_PORT_COUNT];

#if defined(TEST_USB_TYPEC_CTVPD)
static int ct_connect_sink(enum cc_type cc, enum snk_con_voltage_type v)
{
	int ret;

	switch (v) {
	case SRC_CON_DEF:
		ret = (cc) ? mock_set_cc2_rp1a5_odh(PD_SRC_DEF_RD_THRESH_MV) :
				mock_set_cc1_rp1a5_odh(PD_SRC_DEF_RD_THRESH_MV);
		break;
	case SRC_CON_1_5:
		ret = (cc) ? mock_set_cc2_rp1a5_odh(PD_SRC_1_5_RD_THRESH_MV) :
				mock_set_cc1_rp1a5_odh(PD_SRC_1_5_RD_THRESH_MV);
		break;
	case SRC_CON_3_0:
		ret = (cc) ? mock_set_cc2_rp1a5_odh(PD_SRC_3_0_RD_THRESH_MV) :
				mock_set_cc1_rp1a5_odh(PD_SRC_3_0_RD_THRESH_MV);
		break;
	default:
		ret = 0;
	}

	return ret;
}

static int  ct_disconnect_sink(void)
{
	int r1;
	int r2;

	r1 = mock_set_cc1_rp1a5_odh(PD_SRC_DEF_VNC_MV);
	r2 = mock_set_cc1_rp1a5_odh(PD_SRC_DEF_VNC_MV);

	return r1 & r2;
}

static int ct_connect_source(enum cc_type cc, enum vbus_type vbus)
{
	mock_set_ct_vbus(vbus);
	return (cc) ? mock_set_cc2_rpusb_odh(PD_SNK_VA_MV) :
				mock_set_cc1_rpusb_odh(PD_SNK_VA_MV);
}

static int ct_disconnect_source(void)
{
	int r1;
	int r2;

	mock_set_ct_vbus(VBUS_0);
	r1 = mock_set_cc1_rpusb_odh(0);
	r2 = mock_set_cc2_rpusb_odh(0);

	return r1 & r2;
}
#endif

static int host_disconnect_source(void)
{
	mock_set_host_vbus(VBUS_0);
	mock_set_host_cc_source_voltage(0);
	return 1;
}

static int host_connect_source(enum vbus_type vbus)
{
	mock_set_host_vbus(vbus);
	mock_set_host_cc_source_voltage(PD_SNK_VA_MV);
	return 1;
}

#if defined(TEST_USB_TYPEC_CTVPD)
static int host_connect_sink(enum snk_con_voltage_type v)
{
	switch (v) {
	case SRC_CON_DEF:
		mock_set_host_cc_sink_voltage(PD_SRC_DEF_RD_THRESH_MV);
		break;
	case SRC_CON_1_5:
		mock_set_host_cc_sink_voltage(PD_SRC_1_5_RD_THRESH_MV);
		break;
	case SRC_CON_3_0:
		mock_set_host_cc_sink_voltage(PD_SRC_3_0_RD_THRESH_MV);
		break;
	}

	return 1;
}

static int  host_disconnect_sink(void)
{
	mock_set_host_cc_sink_voltage(PD_SRC_DEF_VNC_MV);
	return 1;
}
#endif

static void init_port(int port)
{
	pd_port[port].polarity = 0;
	pd_port[port].rev = PD_REV30;
	pd_port[port].msg_tx_id = 0;
	pd_port[port].msg_rx_id = 0;
}

static int check_host_ra_rd(void)
{
	/* Make sure CC_RP3A0_RD_L is configured as GPO */
	if (mock_get_cfg_cc_rp3a0_rd_l() != PIN_GPO)
		return 0;

	/* Make sure CC_RP3A0_RD_L is asserted low */
	if (mock_get_cc_rp3a0_rd_l() != 0)
		return 0;

	/* Make sure VPDMCU_CC_EN is enabled */
	if (mock_get_mcu_cc_en() != 1)
		return 0;

	/* Make sure CC_VPDMCU is configured as ADC */
	if (mock_get_cfg_cc_vpdmcu() != PIN_ADC)
		return 0;

	/* Make sure CC_DB_EN_OD is HZ */
	if (mock_get_cc_db_en_od() != GPO_HZ)
		return 0;

	return 1;
}

static int check_host_rd(void)
{
	/* Make sure CC_RP3A0_RD_L is configured as GPO */
	if (mock_get_cfg_cc_rp3a0_rd_l() != PIN_GPO)
		return 0;

	/* Make sure CC_RP3A0_RD_L is asserted low */
	if (mock_get_cc_rp3a0_rd_l() != 0)
		return 0;

	/* Make sure VPDMCU_CC_EN is enabled */
	if (mock_get_mcu_cc_en() != 1)
		return 0;

	/* Make sure CC_VPDMCU is configured as ADC */
	if (mock_get_cfg_cc_vpdmcu() != PIN_ADC)
		return 0;

	/* Make sure CC_DB_EN_OD is LOW */
	if (mock_get_cc_db_en_od() != GPO_LOW)
		return 0;

	return 1;
}

#if defined(TEST_USB_TYPEC_CTVPD)
static int check_host_is_isolated(void)
{
	/* Make sure VPDMCU_CC is enabled */
	if (!mock_get_mcu_cc_en())
		return 0;

	/* Make sure CT and Host VBUS is isolated */
	if (mock_get_vbus_pass_en())
		return 0;

	return 1;
}

static int check_host_is_not_isolated(void)
{
	int ct_cc1, ct_cc2;
	int host_cc;

	/* Make sure VPDMCU_CC is disabed */
	if (mock_get_mcu_cc_en())
		return 0;

	/* Make sure host cc is open */
	vpd_host_get_cc(&host_cc);
	if (host_cc != 0)
		return 0;

	/* Make sure CT port’s CC pin is connected to the host port’s CC */
	if (moch_get_ct_cl_sel() == CT_OPEN)
		return 0;

	/* Make sure CT port’s RD on CC1 and CC2 is disabled */
	vpd_ct_get_cc(&ct_cc1, &ct_cc2);
	if (ct_cc1 != 0 && ct_cc2 != 0)
		return 0;

	/* Make sure CT and Host VBUS is isolated */
	if (!mock_get_vbus_pass_en())
		return 0;

	return 1;
}
#endif

void inc_tx_id(int port)
{
	pd_port[port].msg_tx_id = (pd_port[port].msg_tx_id + 1) % 7;
}

void inc_rx_id(int port)
{
	pd_port[port].msg_rx_id = (pd_port[port].msg_rx_id + 1) % 7;
}

static int verify_goodcrc(int port, int role, int id)
{
	return pd_test_tx_msg_verify_sop_prime(port) &&
		pd_test_tx_msg_verify_short(port, PD_HEADER(PD_CTRL_GOOD_CRC,
					role, role, id, 0, 0, 0)) &&
		pd_test_tx_msg_verify_crc(port) &&
		pd_test_tx_msg_verify_eop(port);
}

static void simulate_rx_msg(int port, uint16_t header, int cnt,
							const uint32_t *data)
{
	int i;

	pd_test_rx_set_preamble(port, 1);
	pd_test_rx_msg_append_sop_prime(port);
	pd_test_rx_msg_append_short(port, header);

	crc32_init();
	crc32_hash16(header);

	for (i = 0; i < cnt; ++i) {
		pd_test_rx_msg_append_word(port, data[i]);
		crc32_hash32(data[i]);
	}

	pd_test_rx_msg_append_word(port, crc32_result());

	pd_test_rx_msg_append_eop(port);
	pd_test_rx_msg_append_last_edge(port);

	pd_simulate_rx(port);
}

static void simulate_goodcrc(int port, int role, int id)
{
	simulate_rx_msg(port, PD_HEADER(PD_CTRL_GOOD_CRC, role, role, id, 0,
						pd_port[port].rev, 0), 0, NULL);
}

static void simulate_discovery_identity(int port)
{
	uint16_t header = PD_HEADER(PD_DATA_VENDOR_DEF, PD_ROLE_SOURCE,
					0, pd_port[port].msg_rx_id,
					1, pd_port[port].rev, 0);
	uint32_t msg = VDO(USB_SID_PD,
			1, /* Structured VDM */
			VDO_SVDM_VERS(1) |
			VDO_CMDT(CMDT_INIT) |
			CMD_DISCOVER_IDENT);

	simulate_rx_msg(port, header, 1, (const uint32_t *)&msg);
}

static int test_vpd_host_src_detection(void)
{
	int port = PORT0;

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	/*
	 * TEST:
	 * Host is configured properly and start state is UNATTACHED_SNK
	 */

	TEST_ASSERT(check_host_ra_rd());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	/*
	 * TEST:
	 * Host PORT Source Connection Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host CC debounce in ATTACH_WAIT_SNK state
	 */

	TEST_ASSERT(host_disconnect_source());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(50 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host CC debounce in ATTACH_WAIT_SNK state
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(50 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host Port Connection Removed
	 */
	TEST_ASSERT(host_disconnect_source());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	return EC_SUCCESS;
}

static int test_vpd_host_src_detection_vbus(void)
{
	int port = PORT0;

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	/*
	 * TEST:
	 * Host is configured properly and start state is UNATTACHED_SNK
	 */

	TEST_ASSERT(check_host_ra_rd());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	/*
	 * TEST:
	 * Host Port Source Connection Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host Port Source Detected for tCCDebounce and Host Port VBUS
	 * Detected.
	 */

	TEST_ASSERT(host_connect_source(VBUS_5));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACHED_SNK);

	/*
	 * TEST:
	 * Host Port VBUS Removed
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	TEST_ASSERT(host_disconnect_source());

	return EC_SUCCESS;
}

static int test_vpd_host_src_detection_vconn(void)
{
	int port = PORT0;

	/*
	 * TEST:
	 * Host is configured properly and start state is UNATTACHED_SNK
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(check_host_ra_rd());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	/*
	 * TEST:
	 * Host Source Connection Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host Port Source Detected for tCCDebounce and VCONN Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_3);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACHED_SNK);

	/* VCONN was detected. Make sure RA is removed */
	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);
	TEST_ASSERT(check_host_rd());

	/*
	 * TEST:
	 * Host Port VCONN Removed
	 */

	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	TEST_ASSERT(host_disconnect_source());

	return EC_SUCCESS;
}

static int test_vpd_host_src_detection_message_reception(void)
{
	int port = PORT0;
	uint32_t expected_vdm_header = VDO(USB_VID_GOOGLE,
			1, /* Structured VDM */
			VDO_SVDM_VERS(1) |
			VDO_CMDT(CMDT_RSP_ACK) |
			CMD_DISCOVER_IDENT);
	uint32_t expected_vdo_id_header = VDO_IDH(
			0, /* Not a USB Host */
			1, /* Capable of being enumerated as USB Device */
			IDH_PTYPE_VPD,
			0, /* Modal Operation Not Supported */
			USB_VID_GOOGLE);
	uint32_t expected_vdo_cert = 0;
	uint32_t expected_vdo_product = VDO_PRODUCT(
			CONFIG_USB_PID,
			USB_BCD_DEVICE);
	uint32_t expected_vdo_vpd = VDO_VPD(
			VPD_HW_VERSION,
			VPD_FW_VERSION,
			VPD_MAX_VBUS_20V,
			VPD_VBUS_IMP(VPD_VBUS_IMPEDANCE),
			VPD_GND_IMP(VPD_GND_IMPEDANCE),
#ifdef CONFIG_USB_TYPEC_CTVPD
			VPD_CTS_SUPPORTED
#else
			VPD_CTS_NOT_SUPPORTED
#endif
			);

	/*
	 * TEST:
	 * Host is configured properly and start state is UNATTACHED_SNK
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(check_host_ra_rd());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	/*
	 * Transition to ATTACHED_SNK
	 */

	TEST_ASSERT(host_connect_source(VBUS_5));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACHED_SNK);

	/* Run state machines to enable rx monitoring */
	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	/*
	 * TEST:
	 * Reception of Discovery Identity message
	 */

	simulate_discovery_identity(port);
	task_wait_event(30 * MSEC);

	TEST_ASSERT(verify_goodcrc(port,
				PD_ROLE_SINK, pd_port[port].msg_rx_id));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(30 * MSEC);
	inc_rx_id(port);

	/* Test Discover Identity Ack */
	TEST_ASSERT(pd_test_tx_msg_verify_sop_prime(port));
	TEST_ASSERT(pd_test_tx_msg_verify_short(port,
			PD_HEADER(PD_DATA_VENDOR_DEF, PD_PLUG_CABLE_VPD, 0,
			pd_port[port].msg_tx_id, 5, pd_port[port].rev, 0)));
	TEST_ASSERT(pd_test_tx_msg_verify_word(port, expected_vdm_header));
	TEST_ASSERT(pd_test_tx_msg_verify_word(port, expected_vdo_id_header));
	TEST_ASSERT(pd_test_tx_msg_verify_word(port, expected_vdo_cert));
	TEST_ASSERT(pd_test_tx_msg_verify_word(port, expected_vdo_product));
	TEST_ASSERT(pd_test_tx_msg_verify_word(port, expected_vdo_vpd));
	TEST_ASSERT(pd_test_tx_msg_verify_crc(port));
	TEST_ASSERT(pd_test_tx_msg_verify_eop(port));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(30 * MSEC);

	/* Ack was good. Send GoodCRC */
	simulate_goodcrc(port, PD_ROLE_SOURCE, pd_port[port].msg_tx_id);
	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(30 * MSEC);
	inc_tx_id(port);

	/*
	 * TEST:
	 * Host Port VBUS Removed
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	TEST_ASSERT(host_disconnect_source());

	return EC_SUCCESS;
}


#if defined(TEST_USB_TYPEC_CTVPD)
static int test_ctvpd_drp_toggle(void)
{
	int port = PORT0;

	/*
	 * TEST:
	 * Host and CT are configured properly and start state is UNATTACHED_SNK
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(check_host_ra_rd());
	TEST_ASSERT(check_host_is_isolated());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);
	TEST_ASSERT(ct_connect_source(CC1, VBUS_5));

	/*
	 * TEST:
	 * Power Source is detected and present on the Charge-Through port CC1.
	 * Host Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SRC);

	/*
	 * TEST:
	 * Host Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	TEST_ASSERT(ct_disconnect_source());
	TEST_ASSERT(ct_connect_source(CC2, VBUS_5));

	/*
	 * TEST:
	 * Power Source is detected and present on the Charge-Through port CC2.
	 * Host Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SRC);

	/*
	 * TEST:
	 * Host Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	TEST_ASSERT(ct_disconnect_source());

	return EC_SUCCESS;
}

static int test_ctvpd_attach_wait_src(void)
{
	int port = PORT0;

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	/*
	 * TEST:
	 * Host and CT are configured properly and start state is UNATTACHED_SNK
	 */

	TEST_ASSERT(check_host_ra_rd());
	TEST_ASSERT(check_host_is_isolated());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);
	TEST_ASSERT(ct_connect_source(CC1, VBUS_5));

	/*
	 * TEST:
	 * Power Source is detected and present on the Charge-Through port.
	 * Host Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SRC);

	/*
	 * TEST:
	 * Host Port Sink Connection Detected
	 */
	TEST_ASSERT(host_connect_sink(SRC_CON_DEF));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SRC);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(10 * MSEC);

	/*
	 * TEST:
	 * Host Port Sink Connection Removed
	 */
	TEST_ASSERT(host_disconnect_sink());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(10 * MSEC);

	/*
	 * TEST:
	 * Power Source is detected and present on the Charge-Through port.
	 * Host Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SRC);

	/*
	 * TEST:
	 * Host Port Sink Connection Detected
	 */
	TEST_ASSERT(host_connect_sink(SRC_CON_1_5));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SRC);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(10 * MSEC);

	/*
	 * TEST:
	 * Host Port Sink Connection Removed
	 */
	TEST_ASSERT(host_disconnect_sink());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(10 * MSEC);

	/*
	 * TEST:
	 * Power Source is detected and present on the Charge-Through port.
	 * Host Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SRC);

	/*
	 * TEST:
	 * Host Port Sink Connection Detected
	 */
	TEST_ASSERT(host_connect_sink(SRC_CON_3_0));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SRC);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(10 * MSEC);

	/*
	 * TEST:
	 * Host Port Sink Connection Removed
	 */
	TEST_ASSERT(host_disconnect_sink());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	TEST_ASSERT(ct_disconnect_source());

	return EC_SUCCESS;
}

static int test_ctvpd_try_snk(void)
{
	int port = PORT0;

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	/*
	 * TEST:
	 * Host and CT are configured properly and start state is UNATTACHED_SNK
	 */

	TEST_ASSERT(check_host_ra_rd());
	TEST_ASSERT(check_host_is_isolated());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);
	TEST_ASSERT(ct_connect_source(CC1, VBUS_5));

	/*
	 * TEST:
	 * Power Source is detected and present on the Charge-Through port.
	 * Host Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SRC);

	/*
	 * TEST:
	 * Host Port Sink Connection Detected
	 */
	TEST_ASSERT(host_connect_sink(SRC_CON_DEF));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SRC);

	/*
	 * TEST:
	 * Host Port VBUS at vSafe0V and Host Port Sink Detected for
	 * tCCDebounce
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == TRY_SNK);

	/*
	 * TEST:
	 * Host Port Source Detected for tTryCCDebounce and Host Port
	 * VBUS Detected
	 */
	TEST_ASSERT(host_connect_source(VBUS_5));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(160 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACHED_SNK);

	/*
	 * TEST:
	 * Host Port Source Connection Removed
	 */
	TEST_ASSERT(host_disconnect_source());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	TEST_ASSERT(ct_disconnect_source());

	return EC_SUCCESS;
}

static int test_ctvpd_try_wait_src(void)
{
	int port = PORT0;

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	/*
	 * TEST:
	 * Host and CT are configured properly and start state is UNATTACHED_SNK
	 */

	TEST_ASSERT(check_host_ra_rd());
	TEST_ASSERT(check_host_is_isolated());

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	TEST_ASSERT(ct_connect_source(CC1, VBUS_5));
	TEST_ASSERT(host_disconnect_sink());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	/*
	 * TEST:
	 * Power Source is detected and present on the Charge-Through port.
	 * Host Port DRP Toggle
	 */
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SRC);

	/*
	 * TEST:
	 * Host Port Sink Connection Detected
	 */
	TEST_ASSERT(host_connect_sink(SRC_CON_DEF));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(10 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SRC);

	/*
	 * TEST:
	 * Host Port VBUS at vSafe0V and Host Port Sink Detected for
	 * tCCDebounce
	 */
	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == TRY_SNK);

	/*
	 * TEST:
	 * Host Port Source not Detected for tTryCCDebounce after
	 * tDRPTry
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(160 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == TRY_WAIT_SRC);

	TEST_ASSERT(host_disconnect_sink());

	/*
	 * TEST:
	 * tDRPTry and Host Port Sink not Detected
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(140 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	TEST_ASSERT(ct_disconnect_source());

	return EC_SUCCESS;
}

static int test_ctvpd_attached_src(void)
{
	int port = PORT0;

	/*
	 * TEST:
	 * Host and CT are configured properly and start state is UNATTACHED_SNK
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(check_host_ra_rd());
	TEST_ASSERT(check_host_is_isolated());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);
	TEST_ASSERT(ct_connect_source(CC1, VBUS_5));

	/*
	 * TEST:
	 * Power Source is detected and present on the Charge-Through port.
	 * Host Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SRC);

	/*
	 * TEST:
	 * Host Port SINK Connection Detected
	 */
	TEST_ASSERT(host_connect_sink(SRC_CON_DEF));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SRC);

	/*
	 * TEST:
	 * Host Port VBUS at vSafe0V and Host Port Sink Detected for
	 * tCCDebounce
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == TRY_SNK);

	/*
	 * TEST:
	 * Host Port Source not Detected for tTryCCDebounce after
	 * tDRPTry
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(160 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == TRY_WAIT_SRC);

	/*
	 * Host Port VBUS at vSafe0V and Host Port Sink Detected
	 * for tTryCCDebounce
	 */

	/* host cc debounce */
	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACHED_SRC);

	/*
	 * TEST:
	 * Host Port Sink Removed
	 */

	TEST_ASSERT(host_disconnect_sink());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	/*
	 * TEST:
	 * Power Source is detected and present on the Charge-Through port.
	 * Host Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SRC);

	/*
	 * TEST:
	 * Host Port Connection Detected
	 */
	TEST_ASSERT(host_connect_sink(SRC_CON_DEF));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SRC);

	/*
	 * TEST:
	 * Host Port VBUS at vSafe0V and Host Port Sink Detected for
	 * tCCDebounce
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == TRY_SNK);

	/*
	 * TEST:
	 * Host Port Source not Detected for tTryCCDebounce after
	 * tDRPTry
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(160 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == TRY_WAIT_SRC);

	/*
	 * Host Port VBUS at vSafe0V and Host Port Sink Detected
	 * for tTryCCDebounce
	 */

	/* host cc debounce */
	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACHED_SRC);

	/*
	 * TEST:
	 * CT Port VBUS Removed
	 */

	TEST_ASSERT(ct_connect_source(CC1, VBUS_0));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	TEST_ASSERT(ct_disconnect_source());

	return EC_SUCCESS;
}

static int test_ctvpd_ctunattached_vpd(void)
{
	int port = PORT0;

	/*
	 * TEST:
	 * Host is configured properly and start state is UNATTACHED_SNK
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(check_host_ra_rd());
	TEST_ASSERT(check_host_is_isolated());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	/*
	 * TEST:
	 * Host Source Connection Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host Port Source Detected for tCCDebounce and VCONN Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_3);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACHED_SNK);

	/* VCONN was detected. Make sure RA is removed */
	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);
	TEST_ASSERT(check_host_rd());

	/* TEST: make sure billboard device is presented after PD_T_AME */
	TEST_ASSERT(mock_get_present_billboard() == BB_NONE);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(1010  * MSEC);

	TEST_ASSERT(mock_get_present_billboard() == BB_SNK);

	/*
	 * TEST:
	 * Host Port CC low for tVPDCTDD
	 */

	TEST_ASSERT(host_disconnect_source());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_VPD);

	/*
	 * TEST:
	 * Host Port VCONN Removed
	 */

	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	return EC_SUCCESS;
}

static int test_ctvpd_ctdisabled_vpd(void)
{
	int port = PORT0;

	/*
	 * TEST:
	 * Host is configured properly and start state is UNATTACHED_SNK
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(check_host_ra_rd());
	TEST_ASSERT(check_host_is_isolated());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	/*
	 * TEST:
	 * Host Source Connection Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host Port Source Detected for tCCDebounce and VCONN Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_3);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACHED_SNK);

	/* VCONN was detected. Make sure RA is removed */
	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);
	TEST_ASSERT(check_host_rd());

	/* TEST: make sure billboard support timer can be reset */
	TEST_ASSERT(mock_get_present_billboard() == BB_NONE);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(800 * MSEC);

	TEST_ASSERT(mock_get_present_billboard() == BB_NONE);

	tc_reset_support_timer(port);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(100 * MSEC);

	TEST_ASSERT(mock_get_present_billboard() == BB_NONE);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(1000 * MSEC);

	TEST_ASSERT(mock_get_present_billboard() == BB_SNK);

	/*
	 * TEST:
	 * Host Port CC low for tVPDCTDD
	 */

	TEST_ASSERT(host_disconnect_source());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_VPD);

	/*
	 * TEST:
	 * Charge-Through Port Connection Detected
	 */

	TEST_ASSERT(ct_connect_source(CC2, VBUS_0));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTATTACH_WAIT_VPD);

	/*
	 * TEST:
	 * Charge-Through Port Connection Removed
	 */

	TEST_ASSERT(ct_disconnect_source());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_VPD);

	/*
	 * TEST:
	 * Charge-Through Port Connection Detected
	 */

	TEST_ASSERT(ct_connect_source(CC2, VBUS_0));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTATTACH_WAIT_VPD);

	/*
	 * TEST:
	 * VCONN Removed
	 */

	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTDISABLED_VPD);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	TEST_ASSERT(ct_disconnect_source());

	return EC_SUCCESS;
}

static int test_ctvpd_ctattached_vpd(void)
{
	int port = PORT0;

	/*
	 * TEST:
	 * Host is configured properly and start state is UNATTACHED_SNK
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(check_host_ra_rd());
	TEST_ASSERT(check_host_is_isolated());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	/*
	 * TEST:
	 * Host Source Connection Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host Port Source Detected for tCCDebounce and VCONN Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_3);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACHED_SNK);

	/* VCONN was detected. Make sure RA is removed */
	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);
	TEST_ASSERT(check_host_rd());

	/* TEST: make sure billboard support timer can be reset only once */
	TEST_ASSERT(mock_get_present_billboard() == BB_NONE);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(800 * MSEC);

	TEST_ASSERT(mock_get_present_billboard() == BB_NONE);

	tc_reset_support_timer(port);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(800 * MSEC);

	TEST_ASSERT(mock_get_present_billboard() == BB_NONE);

	tc_reset_support_timer(port);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(250 * MSEC);

	TEST_ASSERT(mock_get_present_billboard() == BB_SNK);

	/*
	 * TEST:
	 * Host Port CC low for tVPDCTDD
	 */

	TEST_ASSERT(host_disconnect_source());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_VPD);

	/*
	 * TEST:
	 * Charge-Through Port Connection Detected
	 */

	TEST_ASSERT(ct_connect_source(CC2, VBUS_0));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTATTACH_WAIT_VPD);

	/*
	 * TEST:
	 * Charge-Through Port Source Detected for tCCDebounce
	 * and Charge-Through Port VBUS Detected
	 */

	TEST_ASSERT(ct_connect_source(CC2, VBUS_5));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(120 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTATTACHED_VPD);

	/* TEST: Make sure Host and CT are connected */
	TEST_ASSERT(check_host_is_not_isolated());

	/*
	 * TEST:
	 * VBUS Removed and CC low for tVPDCTDD
	 */

	TEST_ASSERT(ct_disconnect_source());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_VPD);

	/*
	 * TEST:
	 * Charge-Through Port Connection Detected
	 */

	TEST_ASSERT(ct_connect_source(CC2, VBUS_0));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTATTACH_WAIT_VPD);

	/*
	 * TEST:
	 * Charge-Through Port Source Detected for tCCDebounce
	 * and Charge-Through Port VBUS Detected
	 */

	TEST_ASSERT(ct_connect_source(CC2, VBUS_5));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(115 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTATTACHED_VPD);

	/*
	 * TEST:
	 * VCONN Removed
	 */

	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTDISABLED_VPD);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	TEST_ASSERT(ct_disconnect_source());

	return EC_SUCCESS;
}

static int test_ctvpd_ctunattached_unsupported(void)
{
	int port = PORT0;

	/*
	 * TEST:
	 * Host is configured properly and start state is UNATTACHED_SNK
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(check_host_ra_rd());
	TEST_ASSERT(check_host_is_isolated());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	/*
	 * TEST:
	 * Host Source Connection Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host Port Source Detected for tCCDebounce and VCONN Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_3);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACHED_SNK);

	/* VCONN was detected. Make sure RA is removed */
	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);
	TEST_ASSERT(check_host_rd());

	/*
	 * TEST:
	 * Host Port CC low for tVPDCTDD
	 */

	TEST_ASSERT(host_disconnect_source());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_VPD);

	/*
	 * TEST:
	 * Charge-Through Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_UNSUPPORTED);

	/*
	 * TEST:
	 * Charge-Through Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_VPD);

	/*
	 * TEST:
	 * Charge-Through Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_UNSUPPORTED);

	/*
	 * TEST:
	 * VCONN Removed
	 */

	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	return EC_SUCCESS;
}

static int test_ctvpd_ctattach_wait_unsupported(void)
{
	int port = PORT0;

	/*
	 * TEST:
	 * Host is configured properly and start state is UNATTACHED_SNK
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(check_host_ra_rd());
	TEST_ASSERT(check_host_is_isolated());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	/*
	 * TEST:
	 * Host Source Connection Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host Port Source Detected for tCCDebounce and VCONN Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_3);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACHED_SNK);

	/* VCONN was detected. Make sure RA is removed */
	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);
	TEST_ASSERT(check_host_rd());

	/*
	 * TEST:
	 * Host Port CC low for tVPDCTDD
	 */

	TEST_ASSERT(host_disconnect_source());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_VPD);

	/*
	 * TEST:
	 * Charge-Through Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_UNSUPPORTED);

	/*
	 * TEST:
	 * Charge-Through Port Sink Detected
	 */

	TEST_ASSERT(ct_connect_sink(CC1, SRC_CON_DEF));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(10 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTATTACH_WAIT_UNSUPPORTED);

	/*
	 * TEST:
	 * Charge-Through Port Sink Removed
	 */

	TEST_ASSERT(ct_disconnect_sink());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_VPD);

	/*
	 * TEST:
	 * Charge-Through Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_UNSUPPORTED);

	/*
	 * TEST:
	 * Charge-Through Port Sink Detected
	 */

	TEST_ASSERT(ct_connect_sink(CC1, SRC_CON_DEF));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(10 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTATTACH_WAIT_UNSUPPORTED);

	/*
	 * TEST:
	 * Host Port VCONN Removed
	 */

	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	return EC_SUCCESS;
}

static int test_ctvpd_ctattached_unsupported(void)
{
	int port = PORT0;

	/*
	 * TEST:
	 * Host is configured properly and start state is UNATTACHED_SNK
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(check_host_ra_rd());
	TEST_ASSERT(check_host_is_isolated());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	/*
	 * TEST:
	 * Host Source Connection Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host Port Source Detected for tCCDebounce and VCONN Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_3);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACHED_SNK);

	/* VCONN was detected. Make sure RA is removed */
	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);
	TEST_ASSERT(check_host_rd());

	/*
	 * TEST:
	 * Host Port CC low for tVPDCTDD
	 */

	TEST_ASSERT(host_disconnect_source());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_VPD);

	/*
	 * TEST:
	 * Charge-Through Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_UNSUPPORTED);

	/*
	 * TEST:
	 * Charge-Through Port Sink Detected
	 */

	TEST_ASSERT(ct_connect_sink(CC1, SRC_CON_DEF));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(10 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTATTACH_WAIT_UNSUPPORTED);

	/*
	 * TEST:
	 * Charge-Through Port Sink Detected for tCCDebounce
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTTRY_SNK);

	/*
	 * TEST:
	 * Charge-Through Port Source not Detected after tDRPTryWait
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(730 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTATTACHED_UNSUPPORTED);

	/* TEST:
	 * Charge-Through Port Sink Removed
	 */

	TEST_ASSERT(ct_disconnect_sink());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_VPD);

	/*
	 * TEST:
	 * Charge-Through Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_UNSUPPORTED);

	/*
	 * TEST:
	 * Charge-Through Port Sink Detected
	 */

	TEST_ASSERT(ct_connect_sink(CC1, SRC_CON_DEF));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(10 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTATTACH_WAIT_UNSUPPORTED);

	/*
	 * TEST:
	 * Charge-Through Port Sink Detected for tCCDebounce
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTTRY_SNK);

	/*
	 * TEST:
	 * Charge-Through Port Source Detected for tTryCCDebounce and
	 * Charge-Through Port VBUS Detected
	 */

	TEST_ASSERT(ct_connect_source(CC1, VBUS_5));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(200 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTATTACHED_VPD);

	/*
	 * TEST:
	 * VBUS_REMOVED and CC low for tVPDCTDD
	 */

	TEST_ASSERT(ct_disconnect_source());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_VPD);

	/*
	 * TEST:
	 * Charge-Through Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_UNSUPPORTED);

	/*
	 * TEST:
	 * Charge-Through Port Sink Detected
	 */

	TEST_ASSERT(ct_connect_sink(CC1, SRC_CON_DEF));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(10 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTATTACH_WAIT_UNSUPPORTED);

	/*
	 * TEST:
	 * Charge-Through Port Sink Detected for tCCDebounce
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTTRY_SNK);

	/*
	 * TEST:
	 * Charge-Through Port Source not Detected after tDRPTryWait
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(730 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTATTACHED_UNSUPPORTED);

	/*
	 * TEST:
	 * Host Port VCONN Removed
	 */

	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	return EC_SUCCESS;
}

static int test_ctvpd_cttry_snk(void)
{
	int port = PORT0;

	/*
	 * TEST:
	 * Host is configured properly and start state is UNATTACHED_SNK
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(check_host_ra_rd());
	TEST_ASSERT(check_host_is_isolated());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	/*
	 * TEST:
	 * Host Source Connection Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host Port Source Detected for tCCDebounce and VCONN Detected
	 */

	TEST_ASSERT(host_connect_source(VBUS_0));
	mock_set_vconn(VCONN_3);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACHED_SNK);

	/* VCONN was detected. Make sure RA is removed */
	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);
	TEST_ASSERT(check_host_rd());

	/*
	 * TEST:
	 * Host Port CC low for tVPDCTDD
	 */

	TEST_ASSERT(host_disconnect_source());

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_VPD);

	/*
	 * TEST:
	 * Charge-Through Port DRP Toggle
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTUNATTACHED_UNSUPPORTED);

	/*
	 * TEST:
	 * Charge-Through Port Sink Detected
	 */

	TEST_ASSERT(ct_connect_sink(CC1, SRC_CON_DEF));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(10 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTATTACH_WAIT_UNSUPPORTED);

	/*
	 * TEST:
	 * Charge-Through Port Sink Detected for tCCDebounce
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == CTTRY_SNK);

	/*
	 * TEST:
	 * Host Port VCONN Removed
	 */

	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(140 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	return EC_SUCCESS;
}
#endif

void run_test(void)
{
	test_reset();

	init_port(PORT0);

	/* VPD and CTVPD tests */
	RUN_TEST(test_vpd_host_src_detection);
	RUN_TEST(test_vpd_host_src_detection_vbus);
	RUN_TEST(test_vpd_host_src_detection_vconn);
	RUN_TEST(test_vpd_host_src_detection_message_reception);

	/* CTVPD only tests */
#if defined(TEST_USB_TYPEC_CTVPD)
	RUN_TEST(test_ctvpd_drp_toggle);
	RUN_TEST(test_ctvpd_attach_wait_src);
	RUN_TEST(test_ctvpd_try_snk);
	RUN_TEST(test_ctvpd_try_wait_src);
	RUN_TEST(test_ctvpd_attached_src);
	RUN_TEST(test_ctvpd_ctunattached_vpd);
	RUN_TEST(test_ctvpd_ctdisabled_vpd);
	RUN_TEST(test_ctvpd_ctattached_vpd);
	RUN_TEST(test_ctvpd_ctunattached_unsupported);
	RUN_TEST(test_ctvpd_ctattach_wait_unsupported);
	RUN_TEST(test_ctvpd_ctattached_unsupported);
	RUN_TEST(test_ctvpd_cttry_snk);
#endif
	test_print_result();
}

