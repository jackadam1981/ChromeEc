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
	int rv;
	union data_status_reg status;
	union data_control_reg control = { 0 };

	/* Clear the interrupt */
	control.i2c_int_ack = 1;
	rv = pd_write(port, &control);
	if (rv) {
		CPRINTS("P%d write Err=%d", port, rv);
		return;
	}

	/* Read the status register */
	rv = pd_read(port, &status);
	if (rv) {
		CPRINTS("P%d read Err=%d", port, rv);
		return;
	}

	/* Nothing to do if the data in the status register has not changed */
	if (!memcmp(&status.raw_value[0], &data_status[port].raw_value[0],
		    sizeof(union data_status_reg)))
		return;

	/* Update the new data */
	memcpy(&data_status[port], &status, sizeof(union data_status_reg));

	/* Process MUX events */
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

#ifdef CONFIG_PLATFORM_EC_CONSOLE_CMD_INTEL_ALTMODE
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
		if (rv)
			return rv;

		ccprintf("RD_VAL: ");
		for (i = 0; i < DATA_STATUS_REG_LEN; i++)
			ccprintf("[%d]0x%x, ", i, data.raw_value[i]);
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
		if (rv)
			return rv;

		ccprintf("WR_VAL: ");
		for (i = 0; i < DATA_CONTROL_REG_LEN; i++)
			ccprintf("[%d]0x%x, ", i, control.raw_value[i]);
		ccprintf("\n");
	}

	return rv;
}
DECLARE_CONSOLE_COMMAND(altmode, console_command_intel_altmode,
			"<port> r\n"
			"<port> w <val1> | <val2>",
			"Read or write to PD reg");
#endif /* CONFIG_PLATFORM_EC_CONSOLE_CMD_INTEL_ALTMODE */
