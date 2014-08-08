/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test USB PD module.
 */

#include "common.h"
#include "crc.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "usb_pd.h"
#include "usb_pd_config.h"

#define BMC_SUPPORTED (1 << 15)

struct pd_port_t {
	int host_mode;
	int cc_volt[2]; /* -1 for Hi-Z */
	int has_vbus;
	int msg_id;
} pd_port[PD_PORT_COUNT];

void pd_test_rx_set_preamble(int port, int has_preamble);
void pd_test_rx_msg_append_bits(int port, uint32_t bits, int nb);
void pd_test_rx_msg_append_kcode(int port, uint8_t kcode);
void pd_test_rx_msg_append_4b(int port, uint8_t val);
void pd_test_rx_msg_append_short(int port, uint16_t val);
void pd_test_rx_msg_append_word(int port, uint32_t val);
void pd_simulate_rx(int port);

int pd_test_tx_msg_verify_kcode(int port, uint8_t kcode);
int pd_test_tx_msg_verify_sop(int port);
int pd_test_tx_msg_verify_eop(int port);
int pd_test_tx_msg_verify_4b5b(int port, uint8_t b4);
int pd_test_tx_msg_verify_short(int port, uint16_t val);
int pd_test_tx_msg_verify_word(int port, uint32_t val);

void inc_id(int port)
{
	pd_port[port].msg_id = (pd_port[port].msg_id + 1) % 7;
}

static void init_ports(void)
{
	int i;

	for (i = 0; i < PD_PORT_COUNT; ++i) {
		pd_port[i].host_mode = 0;
		pd_port[i].cc_volt[0] = pd_port[i].cc_volt[1] = -1;
		pd_port[i].has_vbus = 0;
	}
}

int pd_adc_read(int port, int cc)
{
	int val = pd_port[port].cc_volt[cc];
	if (val == -1)
		return pd_port[port].host_mode ? 3000 : 0;
	return val;
}

int pd_snk_is_vbus_provided(int port)
{
	return pd_port[port].has_vbus;
}

void pd_set_host_mode(int port, int enable)
{
	pd_port[port].host_mode = enable;
}

static void simulate_source_cap(int port)
{
	uint16_t header = BMC_SUPPORTED |
			  (pd_src_pdo_cnt << 12) |
			  (pd_port[port].msg_id << 9) |
			  (1 << 8) | /* Source */
			  (0 << 6) | /* Spec Rev */
			  (1 << 0);  /* Type: Source Cap */
	int i;

	pd_test_rx_set_preamble(port, 1);
	pd_test_rx_msg_append_kcode(port, 0x18); /* PD_SYNC1 */
	pd_test_rx_msg_append_kcode(port, 0x18); /* PD_SYNC1 */
	pd_test_rx_msg_append_kcode(port, 0x18); /* PD_SYNC1 */
	pd_test_rx_msg_append_kcode(port, 0x11); /* PD_SYNC2 */

	pd_test_rx_msg_append_short(port, header);

	crc32_init();
	crc32_hash16(header);
	for (i = 0; i < pd_src_pdo_cnt; ++i) {
		pd_test_rx_msg_append_word(port, pd_src_pdo[i]);
		crc32_hash32(pd_src_pdo[i]);
	}
	pd_test_rx_msg_append_word(port, crc32_result());

	pd_test_rx_msg_append_kcode(port, 0x0D); /* PD_EOP */

	pd_simulate_rx(port);
}

static void plug_in_source(int port)
{
	pd_port[port].has_vbus = 1;
	pd_port[port].cc_volt[0] = 3000;
	task_wake(PORT_TO_TASK_ID(port));
	usleep(30 * MSEC);
	simulate_source_cap(port);
}

static void plug_in_sink(int port)
{
	pd_port[port].has_vbus = 0;
	pd_port[port].cc_volt[0] = 0;
}

static void unplug(int port)
{
	pd_port[port].has_vbus = 0;
	pd_port[port].cc_volt[0] = -1;
	pd_port[port].cc_volt[1] = -1;
}

static int test_1(void)
{
	plug_in_source(0);
	task_wait_event(100 * MSEC); /* TODO: How long do we wait for request? */
	TEST_ASSERT(pd_test_tx_msg_verify_sop(0));
	TEST_ASSERT(pd_test_tx_msg_verify_short(0,
			BMC_SUPPORTED | (1 << 12) | (pd_port[0].msg_id << 9) |
			(0 << 8) | (0 << 6) | (2 << 0)));
	TEST_ASSERT(pd_test_tx_msg_verify_word(0, RDO_FIXED(2, 450, 900, 0)));
	return EC_SUCCESS;
}

static int test_2(void)
{
	plug_in_sink(1);
	unplug(1);
	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();
	init_ports();
	pd_set_dual_role(PD_DRP_TOGGLE_ON);

	RUN_TEST(test_2);
	RUN_TEST(test_1);

	test_print_result();
}
