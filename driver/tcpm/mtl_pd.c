/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Meteorlake compatible USB PD driver
 * https://cdrdv2.intel.com/v1/dl/getContent/634442
 */

#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "hooks.h"
#include "i2c.h"
#include "mtl_pd.h"
#include "timer.h"
#include "usb_pd.h"
#include "usb_pd_tbt.h"
#include "util.h"

#include <string.h>

static union mtl_data_status g_data_status[CONFIG_USB_PD_PORT_MAX_COUNT];
const int i2c_addr[] = { 0x50, 0x51 };
const int i2c_port = I2C_PORT_SMLINK;

static int mtl_pd_read(int port, int addr, union mtl_data_status *data)
{
	int rv;
	uint8_t buf[DATA_STATUS_REG_LEN + 1];
	const uint8_t reg = REG_DATA_STATUS;
	ccprintf("%s\n", __func__);
	/*
	 * Read sequence
	 * DEV_ADDR(w) - REG_ADDR - repeated start - DEV_ADDR(r) - DATA0 ..
	 * DATAn
	 */
	rv = i2c_xfer(port, addr, &reg, 1, buf, DATA_STATUS_REG_LEN + 1);
	if (rv)
		return rv;
	if (buf[0] != DATA_STATUS_REG_LEN)
		return EC_ERROR_UNKNOWN;

	memcpy(data, &buf[1], DATA_STATUS_REG_LEN);
	return EC_SUCCESS;
}

static int mtl_pd_write(int port, int addr, union mtl_data_control *data)
{
	uint8_t buf[DATA_CONTROL_REG_LEN + 2];

	buf[0] = REG_DATA_CONTROL;
	buf[1] = DATA_CONTROL_REG_LEN;
	memcpy(&buf[2], data->raw_value, DATA_CONTROL_REG_LEN);

	ccprintf("%s\n", __func__);
	/*
	 * Write sequence
	 * DEV_ADDR(w) - REG_ADDR - DATA_LEN - DATA0 .. DATAn
	 */
	return i2c_xfer(port, addr, buf, DATA_CONTROL_REG_LEN + 2, NULL, 0);
}

//! memcmp(&g_data_status[port].raw_value[0], &status_data.raw_value[0],
//! sizeof(union mtl_data_status)))
static bool pd_data_changed(uint8_t *src, uint8_t *dst)
{
	int i;

	for (i = 0; i < sizeof(union mtl_data_status); i++) {
		if (src[i] != dst[i]) {
			ccprintf("differ@%d 0x%x 0x%x\n", i, src[i], dst[i]);
			return true;
		}
	}
	return false;
}

static void mtl_pd_process_data(int port)
{
	union mtl_data_status status_data;
	union mtl_data_control control_data = { 0 };
	mux_state_t mux = USB_PD_MUX_NONE;

	/* clear the interrupt */
	control_data.i2c_int_ack = 1;
	mtl_pd_write(i2c_port, i2c_addr[port], &control_data);

	/* check th estatus */
	if (!mtl_pd_read(i2c_port, i2c_addr[port], &status_data) &&
	    pd_data_changed(&g_data_status[port].raw_value[0],
			    &status_data.raw_value[0])) {
		ccprintf("PD event at P%d\n", port);
		memcpy(&g_data_status[port], &status_data,
		       sizeof(union mtl_data_status));

		{
			int i;
			for (i = 0; i < DATA_STATUS_REG_LEN; i++)
				ccprintf("0x%x 0x%x\n",
					 g_data_status[port].raw_value[i],
					 status_data.raw_value[i]);
		}

		ccprintf("\n\n");

		/* set the mux */
		if (g_data_status[port].usb2 || g_data_status[port].usb3_2)
			mux = USB_PD_MUX_USB_ENABLED;

		if (g_data_status[port].dp) {
			mux |= USB_PD_MUX_DP_ENABLED;
		}

		if (g_data_status[port].dp_irq) {
			mux |= USB_PD_MUX_HPD_IRQ;
		}

		if (g_data_status[port].dp_hpd) {
		}

		if (pd_get_polarity(port))
			mux |= USB_PD_MUX_POLARITY_INVERTED;

		ccprintf("p%d mux=0x%x\n", port, mux);
		usb_mux_set(port, mux,
			    mux == USB_PD_MUX_NONE ? USB_SWITCH_DISCONNECT :
						     USB_SWITCH_CONNECT,
			    polarity_rm_dts(pd_get_polarity(port)));

		if (g_data_status[port].hpd_lvl) {
			mux |= USB_PD_MUX_HPD_LVL;
			usb_mux_hpd_update(port, USB_PD_MUX_HPD_LVL);
		}

#if 0
		mux = 0;

		if (g_data_status[port].dp) {
			mux |= USB_PD_MUX_DP_ENABLED;
		}

		if (g_data_status[port].dp_irq) {
			mux |= USB_PD_MUX_HPD_IRQ;
		}

                if (g_data_status[port].dp_hpd) {
		}

		if (g_data_status[port].hpd_lvl) {
			mux |= USB_PD_MUX_HPD_LVL;
		}

		if (pd_get_polarity(port))
			mux |= USB_PD_MUX_POLARITY_INVERTED;

		if (mux != 0) {
			usb_mux_set(port, USB_PD_MUX_SAFE_MODE, mux == USB_PD_MUX_NONE ?
				USB_SWITCH_DISCONNECT : USB_SWITCH_CONNECT,
				polarity_rm_dts(pd_get_polarity(port)));

			usb_mux_set(port, mux, mux == USB_PD_MUX_NONE ?
				USB_SWITCH_DISCONNECT : USB_SWITCH_CONNECT,
				polarity_rm_dts(pd_get_polarity(port)));
		}
#endif
	}
}

void mtl_pd_interrupt(enum gpio_signal signal)
{
	task_wake(TASK_ID_MTLPD_TASK);
}

void mtl_pd_task(void *u)
{
	int i;
	// union mtl_data_control control_data = {0};

	while (1) {
//		msleep(1000);
//		mtl_pd_write(i2c_port, i2c_addr[0], &control_data);
#if 1
		task_wait_event(-1);
		ccprintf("***************PD interrupt\n");
		// for (i = 0; i < board_get_usb_pd_port_count(); i++)
		for (i = 0; i < 1; i++) {
			mtl_pd_process_data(i);
		}
#endif
	}
}

/*****************************************************************************/
void pd_request_data_swap(int port)
{
}

enum tcpc_cc_polarity pd_get_polarity(int port)
{
	ccprintf("orinetation %d %d\n", port, g_data_status[port].conn_ori);
	return g_data_status[port].conn_ori;
}

/* TODO start: Get from PD spec */
enum pd_power_role pd_get_power_role(int port)
{
	ccprintf("prole %d %d\n", port, !g_data_status[port].dp_src_snk);
	return !g_data_status[port].dp_src_snk;
}

uint8_t pd_get_task_state(int port)
{
	return 0;
}

int pd_comm_is_enabled(int port)
{
	return 1;
}

bool pd_get_vconn_state(int port)
{
	return true;
}

bool pd_get_partner_dual_role_power(int port)
{
	return false;
}

bool pd_get_partner_data_swap_capable(int port)
{
	return false;
}

bool pd_get_partner_usb_comm_capable(int port)
{
	return false;
}

bool pd_get_partner_unconstr_power(int port)
{
	return false;
}

const char *pd_get_task_state_name(int port)
{
	return "";
}

enum pd_cc_states pd_get_task_cc_state(int port)
{
	return PD_CC_UFP_ATTACHED;
}

enum tbt_compat_rounded_support get_tbt_rounded_support(int port)
{
	return TBT_GEN3_NON_ROUNDED;
}
/* TODO end */

bool pd_capable(int port)
{
	return true;
}

int pd_is_connected(int port)
{
	return g_data_status[port].data_conn;
}

enum pd_data_role pd_get_data_role(int port)
{
	return !g_data_status[port].data_role;
}

__override uint8_t get_dp_pin_mode(int port)
{
	return g_data_status[port].dp_pin << 2;
}

enum tbt_compat_cable_speed get_tbt_cable_speed(int port)
{
	return g_data_status[port].cable_speed;
}

static int console_command_mtl_pd(int argc, const char **argv)
{
	int port, rv, i;
	char rw, *e;
	uint16_t val1;
	uint32_t val2 = 0;
	union mtl_data_status status_data;
	union mtl_data_control control_data;

	/* Get PD port number */
	port = strtoi(argv[1], &e, 0);
	if (*e || port > board_get_usb_pd_port_count())
		return EC_ERROR_PARAM1;

	/* Validate r/w selection */
	rw = argv[2][0];
	if (rw != 'w' && rw != 'r')
		return EC_ERROR_PARAM2;

	if (rw == 'r') {
		rv = mtl_pd_read(i2c_port, i2c_addr[port], &status_data);
		ccprintf("RAW_VAL[LB->HB]: ");
		for (i = 0; i < DATA_STATUS_REG_LEN; i++)
			ccprintf("[%d<-%d]0x%x ", 7 + 8 * i, 8 * i,
				 status_data.raw_value[i]);
		ccprintf("\n");
	} else {
		val1 = strtoull(argv[3], &e, 0);
		if (*e)
			return EC_ERROR_PARAM3;
		memcpy(&control_data.raw_value[0], &val1, 2);

		if (argc > 4) {
			val2 = strtoull(argv[4], &e, 0);
			if (*e)
				return EC_ERROR_PARAM4;
		}
		memcpy(&control_data.raw_value[2], &val2, 4);

		rv = mtl_pd_write(i2c_port, i2c_addr[port], &control_data);
		ccprintf("RAW_VAL[LB->HB]: ");
		for (i = 0; i < DATA_CONTROL_REG_LEN; i++)
			ccprintf("[%d<-%d]0x%x ", 7 + 8 * i, 8 * i,
				 control_data.raw_value[i]);
		ccprintf("\n");
	}

	return rv;
}
DECLARE_CONSOLE_COMMAND(pd, console_command_mtl_pd,
			"<port> r\n"
			"<port> w <val1> | <val2>",
			"Read or write to PD reg");

static int dummy_init(const struct usb_mux *me)
{
	return EC_SUCCESS;
}

static int dummy_set_state(const struct usb_mux *me, mux_state_t mux_state,
			   bool *ack_required)
{
	*ack_required = false;
	return EC_SUCCESS;
}

static int dummy_idle_mode(const struct usb_mux *me, bool idle)
{
	return EC_SUCCESS;
}

static int dummy_low_power_mode(const struct usb_mux *me)
{
	return EC_SUCCESS;
}

static bool dummy_fw_update_capable(void)
{
	return false;
}

void dummy_hpd_update(const struct usb_mux *me, mux_state_t hpd_state,
		      bool *ack_required)
{
	*ack_required = false;
}

const struct usb_mux_driver dummy_mtl_pd = {
	.init = dummy_init,
	.set = dummy_set_state,
	.set_idle_mode = dummy_idle_mode,
	.enter_low_power_mode = dummy_low_power_mode,
	.is_retimer_fw_update_capable = dummy_fw_update_capable,
};

#ifdef CONFIG_KEYBOARD_DISCRETE
#include "keyboard_raw.h"

/* KSO mapping for discrete keyboard */
__override const uint8_t it8801_kso_mapping[] = {
	0, 1, 20, 3, 4, 5, 6, 11, 12, 13, 14, 15, 16,
#ifdef CONFIG_KEYBOARD_KEYPAD
	17, 18
#endif
};
BUILD_ASSERT(ARRAY_SIZE(it8801_kso_mapping) == KEYBOARD_COLS_MAX);

test_mockable void keyboard_raw_drive_column(int col)
{
}

test_mockable int keyboard_raw_read_rows(void)
{
	return 0;
}

void keyboard_raw_enable_interrupt(int enable)
{
}

void keyboard_raw_init(void)
{
}

void keyboard_raw_task_start(void)
{
	keyboard_raw_enable_interrupt(1);
}

void keyboard_event_handler(void)
{
	task_wake(TASK_ID_KEYSCAN);
}
#endif
