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
#define SM_TRANSITION_TIME (10*MSEC)
enum cc_type {CC1, CC2};
enum vbus_type {VBUS_0 = 0, VBUS_5 = 5000};
enum vconn_type {VCONN_0 = 0, VCONN_5 = 5000};

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

static void host_disconnect(void)
{
	mock_set_host_vbus(VBUS_0);
	mock_set_cc_vpdmcu(TYPEC_CC_VOLT_OPEN);
}

static int host_connect(enum tcpc_cc_voltage_status v, enum vbus_type vbus)
{
	mock_set_host_vbus(vbus);
	return mock_set_cc_vpdmcu(v);
}

static void init_port(int port)
{
	pd_port[port].polarity = 0;
	pd_port[port].rev = PD_REV30;
	pd_port[port].msg_tx_id = 0;
	pd_port[port].msg_rx_id = 0;
	host_disconnect();
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

	return 1;
}

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

static int test_vpd_host_detection(void)
{
	int port = PORT0;

	/*
	 * TEST:
	 * Host is configured properly and start state is UNATTACHED_SNK
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(30 * MSEC);

	TEST_ASSERT(check_host_rd());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	/*
	 * TEST:
	 * Host PORT Connection Detected
	 */

	TEST_ASSERT(host_connect(TYPEC_CC_VOLT_SNK_3_0, VBUS_0));
	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host CC debounce in ATTACH_WAIT_SNK state
	 */

	host_disconnect();

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(50 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host CC debounce in ATTACH_WAIT_SNK state
	 */

	TEST_ASSERT(host_connect(TYPEC_CC_VOLT_SNK_3_0, VBUS_0));
	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(50 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host Port Connection Removed
	 */

	host_disconnect();

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(110 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	return EC_SUCCESS;
}

static int test_vpd_host_detection_vbus(void)
{
	int port = PORT0;

	/*
	 * TEST:
	 * Host is configured properly and start state is UNATTACHED_SNK
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(30 * MSEC);

	TEST_ASSERT(check_host_rd());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	/*
	 * TEST:
	 * Host Port Connection Detected
	 */

	TEST_ASSERT(host_connect(TYPEC_CC_VOLT_SNK_3_0, VBUS_0));
	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host Port Source Detected for tCCDebounce and Host Port VBUS
	 * Detected.
	 */

	TEST_ASSERT(host_connect(TYPEC_CC_VOLT_SNK_3_0, VBUS_5));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(PD_T_CC_DEBOUNCE + SM_TRANSITION_TIME);

	TEST_ASSERT(get_typec_state_id(port) == ATTACHED_SNK);

	/*
	 * TEST:
	 * Host Port VBUS Removed
	 */

	TEST_ASSERT(host_connect(TYPEC_CC_VOLT_SNK_3_0, VBUS_0));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	host_disconnect();

	return EC_SUCCESS;
}

static int test_vpd_host_detection_vconn(void)
{
	int port = PORT0;

	/*
	 * TEST:
	 * Host is configured properly and start state is UNATTACHED_SNK
	 */

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(30 * MSEC);

	TEST_ASSERT(check_host_rd());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	/*
	 * TEST:
	 * Host Connection Detected
	 */

	TEST_ASSERT(host_connect(TYPEC_CC_VOLT_SNK_3_0, VBUS_0));
	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(20 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	/*
	 * TEST:
	 * Host Port Source Detected for tCCDebounce and VCONN Detected
	 */

	TEST_ASSERT(host_connect(TYPEC_CC_VOLT_SNK_3_0, VBUS_0));
	mock_set_vconn(VCONN_5);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(PD_T_CC_DEBOUNCE + SM_TRANSITION_TIME);

	TEST_ASSERT(get_typec_state_id(port) == ATTACHED_SNK);

	/*
	 * TEST:
	 * Host Port VCONN Removed
	 */

	mock_set_vconn(VCONN_0);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	host_disconnect();

	return EC_SUCCESS;
}

static int test_vpd_host_message_reception(void)
{
	int i;
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
#ifdef CONFIG_USB_TYPEC_VPD_CT
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
	task_wait_event(30 * MSEC);

	TEST_ASSERT(check_host_rd());
	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	/*
	 * Transition to ATTACHED_SNK
	 */

	TEST_ASSERT(host_connect(TYPEC_CC_VOLT_SNK_3_0, VBUS_5));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == ATTACH_WAIT_SNK);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(PD_T_CC_DEBOUNCE + SM_TRANSITION_TIME);

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

	/* RXERR0 Preamble */
	for (i = 0; i < 40; i++) {
		task_wake(PD_PORT_TO_TASK_ID(port));
		task_wait_event(5 * MSEC);
	}

	/*
	 * TEST:
	 * Host Port VBUS Removed
	 */

	TEST_ASSERT(host_connect(TYPEC_CC_VOLT_SNK_3_0, VBUS_0));

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(5 * MSEC);

	TEST_ASSERT(get_typec_state_id(port) == UNATTACHED_SNK);

	host_disconnect();

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	init_port(PORT0);

	/* VPD and CTVPD tests */
	RUN_TEST(test_vpd_host_detection);
	RUN_TEST(test_vpd_host_detection_vbus);
	RUN_TEST(test_vpd_host_detection_vconn);
	RUN_TEST(test_vpd_host_message_reception);

	/* TODO(shurst): CTVPD only tests */

	test_print_result();
}

