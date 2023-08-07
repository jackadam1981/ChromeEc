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
	rv = i2c_xfer(pd_config[port].i2c_info.port,
		      pd_config[port].i2c_info.addr_flags, &reg, 1, buf,
		      DATA_STATUS_REG_LEN + 1);
	if (rv)
		return rv;
	if (buf[0] != DATA_STATUS_REG_LEN)
		return EC_ERROR_UNKNOWN;

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

	/* Nothing to do if the data in the status register has not changed */
	if (pd_read(port, &status) ||
	    memcmp(&status.raw_value[0], &data_status[port].raw_value[0],
		   sizeof(union data_status_reg)))
		return;

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

		for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
			/* Process data of interrupted port */
			if (!gpio_get_level(pd_config[i].alert_signal))
				process_altmode_pd_data(i);
		}
	}
}
