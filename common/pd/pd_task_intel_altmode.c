/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Source file for PD task to configure USB-C Alternate modes on Intel SoC.
 * Elaborate details can be found in respective SoC's "Platform Power
 * Delivery Controller Interface for SoC and Retimer" document.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "pd_task_intel_altmode.h"
#include "task.h"
#include "usb_mux.h"
#include "usb_pd.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ##args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ##args)

/* Store current data of the DATA STATUS register */
static union data_status_reg data_status[CONFIG_USB_PD_PORT_MAX_COUNT];

static int pd_read(int port, union data_status_reg *data)
{
	int rv;
	uint8_t buf[DATA_STATUS_REG_LEN + 1];
	const uint8_t reg = REG_DATA_STATUS;

	/*
	 * Read sequence
	 * DEV_ADDR - REG_ID - DEV_ADDR - READ_LEN - DATA0 .. DATAn
	 */
	ccprintf("i2c=%d add=0x%x, rg=0x%x\n", pd_config[port].i2c_info.port,
		 pd_config[port].i2c_info.addr_flags, reg);
	rv = i2c_xfer(pd_config[port].i2c_info.port,
		      pd_config[port].i2c_info.addr_flags, &reg, 1, buf,
		      DATA_STATUS_REG_LEN + 1);
	ccprintf("p%d @%d rv=%d******\n", port, __LINE__, rv);
	if (rv)
		return rv;
	ccprintf("p%d @%d ******\n", port, __LINE__);
	if (buf[0] != DATA_STATUS_REG_LEN)
		return EC_ERROR_UNKNOWN;

	ccprintf("p%d @%d ******\n", port, __LINE__);
	memcpy(data, &buf[1], DATA_STATUS_REG_LEN);
	return EC_SUCCESS;
}

static int pd_write(int port, union data_control_reg *data)
{
	uint8_t buf[DATA_CONTROL_REG_LEN + 2];

	buf[0] = REG_DATA_CONTROL;
	buf[1] = DATA_CONTROL_REG_LEN;
	memcpy(&buf[2], data->raw_value, DATA_CONTROL_REG_LEN);

	/*
	 * Write sequence
	 * DEV_ADDR - REG_ID - DATA_LEN - DATA0 .. DATAn
	 */
	return i2c_xfer(pd_config[port].i2c_info.port,
			pd_config[port].i2c_info.addr_flags, buf,
			DATA_CONTROL_REG_LEN + 2, NULL, 0);
}

static void process_altmode_pd_data(int port)
{
	union data_status_reg status;
	union data_control_reg control = { 0 };
	mux_state_t mux = USB_PD_MUX_NONE;
	mux_state_t prv_hpd_lvl;

	/* Clear the interrupt */
	control.i2c_int_ack = 1;
	pd_write(port, &control);

	ccprintf("p%d @%d ******\n", port, __LINE__);
	/* Nothing to do if the data in the status register has not changed */
	if (pd_read(port, &status)) {
		ccprintf("p%d @%d ******\n", port, __LINE__);
		return;
	}

	if (!memcmp(&status.raw_value[0], &data_status[port].raw_value[0],
		    sizeof(union data_status_reg))) {
		ccprintf("p%d @%d ******\n", port, __LINE__);
		return;
	}

	ccprintf("p%d @%d ******\n", port, __LINE__);
	/* Store previous HPD tatus */
	prv_hpd_lvl = data_status[port].hpd_lvl;

	/* Update the new data */
	memcpy(&data_status[port], &status, sizeof(union data_status_reg));

	/* Process MUX events */

	/* Orientation */
	if (status.conn_ori)
		mux |= USB_PD_MUX_POLARITY_INVERTED;

	/* USB status */
	if (status.usb2 || status.usb3_2)
		mux |= USB_PD_MUX_USB_ENABLED;

	/* DP status */
	if (status.dp)
		mux |= USB_PD_MUX_DP_ENABLED;

	if (status.hpd_lvl)
		mux |= USB_PD_MUX_HPD_LVL;

	if (status.dp_irq)
		mux |= USB_PD_MUX_HPD_IRQ;

	usb_mux_set(port, mux,
		    mux == USB_PD_MUX_NONE ? USB_SWITCH_DISCONNECT :
					     USB_SWITCH_CONNECT,
		    polarity_rm_dts(status.conn_ori));

	/* Update the change in HPD level */
	if (prv_hpd_lvl != status.hpd_lvl)
		usb_mux_hpd_update(port, USB_PD_MUX_HPD_LVL);
}

/* Enable interrupt when AP is on */
static void enable_pd_irq(void)
{
	int i;

	for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++)
		gpio_enable_interrupt(pd_config[i].alert_signal);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, enable_pd_irq, HOOK_PRIO_DEFAULT);

/* Disable interrupt when AP is down to avoid unnecessary wake of AP */
static void disable_pd_irq(void)
{
	int i;

	for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++)
		gpio_disable_interrupt(pd_config[i].alert_signal);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, disable_pd_irq, HOOK_PRIO_DEFAULT);

void pd_altmode_interrupt(enum gpio_signal signal)
{
	task_wake(TASK_ID_PD_TASK_INTEL_ALTMODE);
}

void pd_task_intel_altmode(void *u)
{
	int i;

	while (1) {
		task_wait_event(-1);
		ccprintf("wake event ******\n");
		for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
			/* Process data of interrupted port */
			if (!gpio_get_level(pd_config[i].alert_signal))
				process_altmode_pd_data(i);
		}
	}
}

static int console_command_intel_altmode(int argc, const char **argv)
{
	int port, rv, i;
	char rw, *e;
	uint16_t val1;
	uint32_t val2 = 0;
	union data_status_reg data;
	union data_control_reg control;

	/* Get PD port number */
	port = strtoi(argv[1], &e, 0);
	if (*e || port > board_get_usb_pd_port_count())
		return EC_ERROR_PARAM1;

	/* Validate r/w selection */
	rw = argv[2][0];
	if (rw != 'w' && rw != 'r')
		return EC_ERROR_PARAM2;

	if (rw == 'r') {
		rv = pd_read(port, &data);
		ccprintf("RAW_VAL[LB->HB]: ");
		for (i = 0; i < DATA_STATUS_REG_LEN; i++)
			ccprintf("[%d<-%d]0x%x ", 7 + 8 * i, 8 * i,
				 data.raw_value[i]);
		ccprintf("\n");
	} else {
		val1 = strtoull(argv[3], &e, 0);
		if (*e)
			return EC_ERROR_PARAM3;

		if (argc > 4) {
			val2 = strtoull(argv[4], &e, 0);
			if (*e)
				return EC_ERROR_PARAM4;
		}

		memcpy(&control.raw_value[0], &val1, 2);
		memcpy(&control.raw_value[2], &val2, 4);

		rv = pd_write(port, &control);
		ccprintf("RAW_VAL[LB->HB]: ");
		for (i = 0; i < DATA_CONTROL_REG_LEN; i++)
			ccprintf("[%d<-%d]0x%x ", 7 + 8 * i, 8 * i,
				 control.raw_value[i]);
		ccprintf("\n");
	}

	return rv;
}
DECLARE_CONSOLE_COMMAND(pd, console_command_intel_altmode,
			"<port> r\n"
			"<port> w <val1> | <val2>",
			"Read or write to PD reg");

/****************************************************************************/
enum tcpc_cc_polarity pd_get_polarity(int port)
{
	return POLARITY_CC1;
}

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
