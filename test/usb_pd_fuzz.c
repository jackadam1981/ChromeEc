/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test USB PD module.
 */
#include "battery.h"
#include "common.h"
#include "crc.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "usb_pd.h"
#include "usb_pd_test_util.h"
#include "util.h"

#include <pthread.h>

#define TASK_EVENT_FUZZ TASK_EVENT_CUSTOM(1)

#define PORT0	0
#define PORT1	1

#define BATTERY_DESIGN_VOLTAGE 7600
#define BATTERY_DESIGN_CAPACITY 5131
#define BATTERY_FULL_CHARGE_CAPACITY 5131
#define BATTERY_REMAINING_CAPACITY 2566

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

/* Mock functions */
#ifdef CONFIG_USB_PD_REV30

uint16_t pd_get_identity_vid(int port)
{
	return 0;
}

uint16_t pd_get_identity_pid(int port)
{
	return 0;
}

enum battery_present battery_is_present(void)
{
	return BP_YES;
}

int battery_status(int *status)
{
	*status = 1;
	return 0;
}

int battery_remaining_capacity(int *capacity)
{
	*capacity = BATTERY_REMAINING_CAPACITY;
	return 0;
}

int battery_full_charge_capacity(int *capacity)
{
	*capacity = BATTERY_FULL_CHARGE_CAPACITY;
	return 0;
}

int battery_design_capacity(int *capacity)
{
	*capacity = BATTERY_DESIGN_CAPACITY;
	return 0;
}

int battery_design_voltage(int *voltage)
{
	*voltage = BATTERY_DESIGN_VOLTAGE;
	return 0;
}

#endif

int pd_adc_read(int port, int cc)
{
	if (pd_port[port].host_mode &&
	    pd_port[port].partner_role == PD_ROLE_SINK)
		/* we are source connected to sink, return Rd/Open */
		return (pd_port[port].partner_polarity == cc) ? 400 : 3000;
	else if (!pd_port[port].host_mode &&
		  pd_port[port].partner_role == PD_ROLE_SOURCE)
		/* we are sink connected to source, return Rp/Open */
		return (pd_port[port].partner_polarity == cc) ? 1700 : 0;
	else if (pd_port[port].host_mode)
		/* no sink on the other side, both CC are opened */
		return 3000;
	else if (!pd_port[port].host_mode)
		/* no source on the other side, both CC are opened */
		return 0;

	/* should never get here */
	return 0;
}

int pd_snk_is_vbus_provided(int port)
{
	return pd_port[port].has_vbus;
}

void pd_set_host_mode(int port, int enable)
{
	pd_port[port].host_mode = enable;
}

void pd_select_polarity(int port, int polarity)
{
	pd_port[port].polarity = polarity;
}

int pd_vdm(int port, int cnt, uint32_t *payload, uint32_t **rpayload)
{
	return 0;
}

int board_select_rp_value(int port, int rp)
{
	return 0;
}

/*
static void plug_in_source(int port, int polarity)
{
	pd_port[port].has_vbus = 1;
	pd_port[port].partner_role = PD_ROLE_SOURCE;
	pd_port[port].partner_polarity = polarity;
}
*/

static void plug_in_sink(int port, int polarity)
{
	pd_port[port].has_vbus = 0;
	pd_port[port].partner_role = PD_ROLE_SINK;
	pd_port[port].partner_polarity = polarity;
}

static void unplug(int port)
{
	pd_port[port].msg_tx_id = 0;
	pd_port[port].msg_rx_id = 0;
	pd_port[port].has_vbus = 0;
	pd_port[port].partner_role = -1;
	task_wake(PD_PORT_TO_TASK_ID(port));
	usleep(30 * MSEC);
}

/* Tests */

static void init_ports(void)
{
	int i;

	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; ++i) {
		pd_port[i].host_mode = 0;
		pd_port[i].partner_role = -1;
		pd_port[i].has_vbus = 0;
#ifdef CONFIG_USB_PD_REV30
		pd_port[i].rev = PD_REV30;
#else
		pd_port[i].rev = PD_REV20;
#endif
	}
}

static void simulate_rx_msg(int port, uint16_t header, int cnt,
			    const uint32_t *data)
{
	int i;

	pd_test_rx_set_preamble(port, 1);
	pd_test_rx_msg_append_sop(port);
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

static pthread_cond_t done_cond;
static pthread_mutex_t lock;

const uint8_t *f_data;
size_t f_size;

void run_test(void)
{
	uint8_t port = PORT1;

	ccprints("Fuzzing task started");
	wait_for_task_started();
	init_ports();

	while (1) {
		task_wait_event_mask(TASK_EVENT_FUZZ, -1);

		pd_set_dual_role(PD_DRP_TOGGLE_ON);

		plug_in_sink(port, 1);
		task_wake(PD_PORT_TO_TASK_ID(port));
		task_wait_event(250 * MSEC);
		ASSERT(pd_port[port].polarity == 1);

		simulate_rx_msg(port, *((uint16_t *)f_data), (f_size-4)/4, (void *)(f_data+4));

		task_wake(PD_PORT_TO_TASK_ID(port));
		task_wait_event(100 * MSEC);

		/* We're done */
		unplug(port);

		task_wake(PD_PORT_TO_TASK_ID(port));
		task_wait_event(100 * MSEC);

		pthread_cond_signal(&done_cond);
	}
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size < 4)
		return 0;

	f_data = data;
	f_size = size;

	task_set_event(TASK_ID_TEST_RUNNER, TASK_EVENT_FUZZ, 0);
	pthread_cond_wait(&done_cond, &lock);

	return 0;
}
