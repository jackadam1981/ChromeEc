/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "hooks.h"
#include "i2c.h"
#include "util.h"
#include "timer.h"
#include "gpio.h"
#include "watchdog.h"
#include "chipset.h"
#include "registers.h"
#include "system.h"

#define SLAVE0 0x10
#define SLAVE1 0x12
#define SLAVE2 0x14

static int read_register(int addr, int offset, int *data)
{
	static int I2C_PORT_TCPC = 1;
	int rc;

	watchdog_reload();
	rc = i2c_read8(I2C_PORT_TCPC, addr, offset, data);
	if (rc != EC_SUCCESS) {
		if ((addr == SLAVE0) && (offset == 0xA0))
			rc = EC_SUCCESS;
	}
	return rc;
}

static int write_register(int addr, int offset, int data)
{
	static int I2C_PORT_TCPC = 1;

	watchdog_reload();
	return i2c_write8(I2C_PORT_TCPC, addr, offset, data);
}

static int ps8751_reset(void)
{
	board_set_tcpc_power_mode(1, 0);
	board_set_tcpc_power_mode(1, 1);

	watchdog_reload();
	msleep(500);

	return EC_SUCCESS;
}

static int ps8751_wake(void)
{
	int data;

	read_register(SLAVE0, 0xa0, &data);
	msleep(10);

	return write_register(0x16, 0xa0, 0x30);
}

static int ps8751_read_version(int *data)
{
	return read_register(SLAVE0, 0x90, data);
}

static int read_version(int *data)
{
	ps8751_reset();
	ps8751_wake();
	return ps8751_read_version(data);
}

static int enable_wp(void)
{
	int data;
	int value = 0x10;

	read_register(SLAVE1, 0xf0, &data);
	if (!data)
		value = 0x00;
	return write_register(SLAVE1, 0x4b, value);
}

static int disable_wp(void)
{
	int data;
	int value = 0x00;

	read_register(SLAVE1, 0xf0, &data);
	if (!data)
		value = 0x10;
	return write_register(SLAVE1, 0x4b, value);
}

static int reset_SPIIF(void)
{
	disable_wp();

	write_register(SLAVE2, 0x90, 0x04);
	write_register(SLAVE2, 0x92, 0x00);
	write_register(SLAVE2, 0x93, 0x05);

	return enable_wp();
}

static int enable_mpu(void)
{
	write_register(SLAVE2, 0xd6, 0x00);
	return EC_SUCCESS;
}

static int disable_mpu(void)
{
	write_register(SLAVE2, 0xd6, 0xc0);
	write_register(SLAVE2, 0xd6, 0x40);
	return reset_SPIIF();
}

static int get_ROM_id(int buf[2])
{
	int status;
	int rc;

	write_register(SLAVE2, 0x90, 0x90);
	write_register(SLAVE2, 0x90, 0x00);
	write_register(SLAVE2, 0x90, 0x00);
	write_register(SLAVE2, 0x90, 0x00);
	write_register(SLAVE2, 0x92, 0x13);
	write_register(SLAVE2, 0x93, 0x01);

	do
		read_register(SLAVE2, 0x9e, &status);
	while (status & 0x01);

	read_register(SLAVE2, 0x91, &buf[0]);
	rc = read_register(SLAVE2, 0x91, &buf[1]);

	ccprintf("ROM ID %x %x (%d)\n", buf[0], buf[1], rc);

	return rc;
}

static int WaitSPIROMReady(void)
{
	int status;

	do
		read_register(SLAVE2, 0x9e, &status);
	while (status & 0x0c);

	do {
		write_register(SLAVE2, 0x90, 0x05);
		write_register(SLAVE2, 0x92, 0x00);
		write_register(SLAVE2, 0x93, 0x01);
		do
			read_register(SLAVE2, 0x93, &status);
		while (status & 0x01);
		read_register(SLAVE2, 0x91, &status);
	} while (status & 0x01);

	return EC_SUCCESS;
}

static int read_status_register(int *data)
{
	write_register(SLAVE2, 0x90, 0x01);
	write_register(SLAVE2, 0x92, 0x00);
	write_register(SLAVE2, 0x93, 0x01);
	return read_register(SLAVE2, 0x91, data);
}

static int write_status_register(int data, int access)
{
	ccprintf("write status register: %d %d\n", data, access);
	disable_wp();
	write_register(SLAVE2, 0x90, 0x01);
	write_register(SLAVE2, 0x90, data);
	write_register(SLAVE2, 0x92, access);
	write_register(SLAVE2, 0x93, 0x05);
	return enable_wp();
}

static int enable_write_status_register(int data)
{
	disable_wp();
	write_register(SLAVE2, 0x90, data);
	write_register(SLAVE2, 0x92, 0x00);
	write_register(SLAVE2, 0x93, 0x05);
	return enable_wp();
}

static int spi_write_enable(void)
{
	return enable_write_status_register(0x06);
}

static int enable_write(int access)
{
	int retry;
	int val;

	disable_mpu();
	enable_write_status_register(0x06);
	write_status_register(0x00, access);

	WaitSPIROMReady();

	read_status_register(&val);
	if (val != 0) {
		ccprintf("read status 1: %x\n", val);
		return EC_ERROR_INVAL;
	}

	disable_wp();

	for (retry = 0; retry < 20; retry++) {
		write_register(SLAVE2, 0xda, 0xaa);
		write_register(SLAVE2, 0xda, 0x55);
		write_register(SLAVE2, 0xda, 0x50);
		write_register(SLAVE2, 0xda, 0x41);
		write_register(SLAVE2, 0xda, 0x52);
		write_register(SLAVE2, 0xda, 0x44);

		read_register(SLAVE2, 0xda, &val);
		if (val == 0x01)
			return EC_SUCCESS;
		ccprintf("read status 2: %x\n", val);
	}
	return EC_ERROR_INVAL;
}

static int disable_write(void)
{
	int status;

	disable_mpu();
	enable_write_status_register(0x06);
	write_status_register(0x9c, 0x01);

	WaitSPIROMReady();

	read_status_register(&status);
	if ((status & 0x9c) != 0x9c)
		return EC_ERROR_INVAL;
	return write_register(SLAVE2, 0xda, 0x00);
}

static int enter_fwu_mode(void)
{
	ps8751_reset();
	ps8751_wake();
	disable_mpu();
	disable_wp();
	spi_write_enable();
	enable_write(0x01);
	return EC_SUCCESS;
}

static int setup_address(int address, int access)
{
	write_register(SLAVE2, 0x90, access);
	write_register(SLAVE2, 0x90, (address >> 16) & 0xffu);
	write_register(SLAVE2, 0x90, (address >>  8) & 0xffu);
	write_register(SLAVE2, 0x90, (address >>  0) & 0xffu);
	return EC_SUCCESS;
}

static int SPISectorErase(int address)
{
	spi_write_enable();

	setup_address(address, 0x20);
	write_register(SLAVE2, 0x92, 0x03);
	write_register(SLAVE2, 0x93, 0x05);

	WaitSPIROMReady();

	return EC_SUCCESS;
}

static int erase(int start, int end, int step)
{
	disable_mpu();
	disable_wp();
	spi_write_enable();
	enable_write(0x01);

	while (start < end) {
		ccprintf("Erasing %x..%x\n", start, start + step - 1);
		SPISectorErase(start);
		start += step;
	}

	return EC_SUCCESS;
}

static int program(int address, const uint8_t data[8])
{
	int i;

	spi_write_enable();
	setup_address(address, 0x02);

	for (i = 0; i < 8; ++i)
		write_register(SLAVE2, 0x90, data[i]);

	write_register(SLAVE2, 0x92, 0x0b);
	write_register(SLAVE2, 0x93, 0x05);

	WaitSPIROMReady();

	return EC_SUCCESS;
}

static int read_flash(int address, unsigned char buf[8])
{
	int status;
	int i;

	ps8751_reset();
	ps8751_wake();

	disable_mpu();
	disable_wp();

	enable_write(0x00);
	spi_write_enable();

	setup_address(address, 0x03);
	write_register(SLAVE2, 0x92, 0x73);
	write_register(SLAVE2, 0x93, 0x01);

	do
		read_register(SLAVE2, 0x9e, &status);
	while (status & 0x01);

	for (i = 0; i < 8; i++) {
		read_register(SLAVE2, 0x91, &status);
		ccprintf("%02x ", status);
		buf[i] = status;
	}
	ccprintf("\n");

	enable_wp();
	enable_mpu();

	return EC_SUCCESS;
}

int ps8751_fw_update(struct host_cmd_handler_args *args)
{
	const struct ec_params_usb_tcpc_fw_update *in = args->params;
	struct ec_response_usb_tcpc_fw_update *out = args->response;
	int address;

	switch (in->cmd) {
	case TCPC_FWU_IDENTIFY:
		args->response_size = sizeof(*out);
		read_version(&out->version);
		get_ROM_id(out->ROM_ID);
		args->result = EC_RES_SUCCESS;
		break;
	case TCPC_FWU_DUMP:
		args->response_size = sizeof(*out);
		read_flash(in->address, out->buffer);
		break;
	case TCPC_FWU_PREPARE:
		enter_fwu_mode();
		break;
	case TCPC_FWU_ERASE:
		erase(0x30000, 0x3f000, 0x1000);
		break;
	case TCPC_FWU_PROGRAM:
		address = 0x30000 + in->address;
		return program(address, in->data);
	case TCPC_FWU_FINALIZE:
		disable_write();
		enable_wp();
		enable_mpu();
		break;
	}
	return EC_SUCCESS;
}

