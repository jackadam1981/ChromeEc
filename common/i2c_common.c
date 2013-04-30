/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* I2C cross-platform code for Chrome EC */

#include "clock.h"
#include "console.h"
#include "host_command.h"
#include "i2c.h"
#include "system.h"
#include "task.h"
#include "util.h"
#include "watchdog.h"

#define CPUTS(outstr) cputs(CC_I2C, outstr)
#define CPRINTF(format, args...) cprintf(CC_I2C, format, ## args)

extern const struct i2c_port_t i2c_ports[I2C_PORTS_USED];

static struct mutex port_mutex[I2C_PORT_COUNT];

void i2c_lock(int port, int lock)
{
	if (lock) {
#ifdef CHIP_stm32
		/* Don't allow deep sleep when I2C port is locked */
		disable_sleep(SLEEP_MASK_I2C);
#endif
		mutex_lock(port_mutex + port);
	} else {
		mutex_unlock(port_mutex + port);
#ifdef CHIP_stm32
		/* Allow deep sleep again after I2C port is unlocked */
		enable_sleep(SLEEP_MASK_I2C);
#endif
	}
}

int i2c_read16(int port, int slave_addr, int offset, int *data)
{
	int rv;
	uint8_t reg, buf[2];

	reg = offset & 0xff;
	/* I2C read 16-bit word: transmit 8-bit offset, and read 16bits */
	i2c_lock(port, 1);
	rv = i2c_xfer(port, slave_addr, &reg, 1, buf, 2, I2C_XFER_SINGLE);
	i2c_lock(port, 0);

	if (rv)
		return rv;

	if (slave_addr & I2C_FLAG_BIG_ENDIAN)
		*data = ((int)buf[0] << 8) | buf[1];
	else
		*data = ((int)buf[1] << 8) | buf[0];

	return EC_SUCCESS;
}

int i2c_write16(int port, int slave_addr, int offset, int data)
{
	int rv;
	uint8_t buf[3];

	buf[0] = offset & 0xff;

	if (slave_addr & I2C_FLAG_BIG_ENDIAN) {
		buf[1] = (data >> 8) & 0xff;
		buf[2] = data & 0xff;
	} else {
		buf[1] = data & 0xff;
		buf[2] = (data >> 8) & 0xff;
	}

	i2c_lock(port, 1);
	rv = i2c_xfer(port, slave_addr, buf, 3, NULL, 0, I2C_XFER_SINGLE);
	i2c_lock(port, 0);

	return rv;
}

int i2c_read8(int port, int slave_addr, int offset, int *data)
{
	int rv;
	/* We use buf[1] here so it's aligned for DMA on STM32 */
	uint8_t reg, buf[1];

	reg = offset;

	i2c_lock(port, 1);
	rv = i2c_xfer(port, slave_addr, &reg, 1, buf, 1, I2C_XFER_SINGLE);
	i2c_lock(port, 0);

	if (!rv)
		*data = buf[0];

	return rv;
}

int i2c_write8(int port, int slave_addr, int offset, int data)
{
	int rv;
	uint8_t buf[2];

	buf[0] = offset;
	buf[1] = data;

	i2c_lock(port, 1);
	rv = i2c_xfer(port, slave_addr, buf, 2, 0, 0, I2C_XFER_SINGLE);
	i2c_lock(port, 0);

	return rv;
}

/*****************************************************************************/
/* Host commands */

/* TODO: replace with single I2C passthru command */

static int i2c_command_read(struct host_cmd_handler_args *args)
{
	const struct ec_params_i2c_read *p = args->params;
	struct ec_response_i2c_read *r = args->response;
	int data, rv = -1;

#ifdef CONFIG_I2C_PASSTHRU_RESTRICTED
	if (system_is_locked())
		return EC_RES_ACCESS_DENIED;
#endif

	if  (p->read_size == 16)
		rv = i2c_read16(p->port, p->addr, p->offset, &data);
	else if (p->read_size == 8)
		rv = i2c_read8(p->port, p->addr, p->offset, &data);

	if (rv)
		return EC_RES_ERROR;
	r->data = data;
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_I2C_READ, i2c_command_read, EC_VER_MASK(0));

static int i2c_command_write(struct host_cmd_handler_args *args)
{
	const struct ec_params_i2c_write *p = args->params;
	int rv = -1;

#ifdef CONFIG_I2C_PASSTHRU_RESTRICTED
	if (system_is_locked())
		return EC_RES_ACCESS_DENIED;
#endif

	if (p->write_size == 16)
		rv = i2c_write16(p->port, p->addr, p->offset, p->data);
	else if (p->write_size == 8)
		rv = i2c_write8(p->port, p->addr, p->offset, p->data);

	if (rv)
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_I2C_WRITE, i2c_command_write, EC_VER_MASK(0));

/* TODO: remove temporary extra debugging for help host-side debugging */
#ifdef CONFIG_I2C_DEBUG_PASSTHRU
#define PTHRUPRINTF(format, args...) cprintf(CC_I2C, format, ## args)
#else
#define PTHRUPRINTF(format, args...)
#endif

static int i2c_command_passthru(struct host_cmd_handler_args *args)
{
	const uint8_t *p = args->params;
	uint8_t *r = args->response;
	int pnext = 0;
	int rnext = 0;
	int port;
	int slave_addr = -1;
	int need_stop = 0;
	int rv = EC_RES_INVALID_PARAM;

#ifdef CONFIG_I2C_PASSTHRU_RESTRICTED
	if (system_is_locked())
		return EC_RES_ACCESS_DENIED;
#endif

	/* Parse and lock port */
	if (pnext >= args->params_size)
		return EC_RES_INVALID_PARAM;

	port = p[pnext++];

	if (port >= I2C_PORT_COUNT)
		return EC_RES_INVALID_PARAM;

	i2c_lock(port, 1);

	/* Loop and process messages */
	while (pnext < args->params_size) {
		int flags = p[pnext++];
		int xferflags = I2C_XFER_START;
		int read_len = 0, write_len = 0;

		PTHRUPRINTF("[%T i2c passthru flags=0x%02x]\n", flags);

		/* Parse slave address if necessary */
		if (flags & EC_I2C_FLAG_10BIT) {
			/* 10-bit addressing not supported yet */
			PTHRUPRINTF("[%T i2c passthru no 10-bit addressing]\n");
			goto passthru_error;

		} else if (flags & EC_I2C_FLAG_7BIT) {
			if (pnext >= args->params_size) {
				PTHRUPRINTF("[%T i2c passthru trunc addr]\n");
				goto passthru_error;
			}

			/* Convert to 8-bit slave address */
			slave_addr = (p[pnext++]) << 1;

		} else if (slave_addr < 0) {
			/* Can't proceed without a slave address */
			PTHRUPRINTF("[%T i2c passthru missing addr]\n");
			goto passthru_error;
		}

		PTHRUPRINTF("[%T i2c passthru port=%d addr=0x%02x\n]",
			port, slave_addr);

		/* Allow skipping start if last message didn't send a stop */
		if (need_stop && (flags & EC_I2C_FLAG_NOSTART))
			xferflags &= ~I2C_XFER_START;

		/* Parse read/write length */
		if (pnext >= args->params_size) {
			PTHRUPRINTF("[%T i2c passthru missing len]\n");
			goto passthru_error;
		}

		/* Make sure we have enough data */
		if (flags & EC_I2C_FLAG_READ) {
			read_len = p[pnext++];

			/* Response is status+count+data */
			if (args->response_max - rnext < (read_len + 2)) {
				PTHRUPRINTF("[%T i2c passthru overflow1]\n");
				goto passthru_error;
			}
		} else {
			write_len = p[pnext++];

			/* Must have bytes to write, and a 1-byte response */
			if (write_len > args->params_size - pnext ||
			    args->response_max - rnext < 1) {
				PTHRUPRINTF("[%T i2c passthru overflow2]\n");
				goto passthru_error;
			}
		}

		/* Set stop bit for last message or if requested */
		if (pnext + write_len == args->params_size ||
		    flags & EC_I2C_FLAG_STOP)
			xferflags |= I2C_XFER_STOP;

		/* Transfer next message */
		PTHRUPRINTF("[%T i2c passthru w=%d r=%d]\n", write_len,
			    read_len);
		rv = i2c_xfer(port, slave_addr,
			      p + pnext, write_len,
			      r + rnext + 2, read_len, xferflags);

		/* Advance over written data, if any */
		pnext += write_len;

		PTHRUPRINTF("[%T i2c passthru rv=%d]\n", rv);

		/* Handle failure */
		if (rv) {
			need_stop = 0;  /* Transfer failures always send stop */

			r[rnext] = EC_I2C_STATUS_ERROR;

			if (rv == EC_ERROR_TIMEOUT)
				r[rnext] |= EC_I2C_STATUS_TIMEOUT;

			/* TODO: parse other errors into status flags */

			/* Add error response to status */
			rnext++;

			/* Stop processing messages */
			break;
		} else if (flags & EC_I2C_FLAG_READ) {
			/* Might not have sent a stop */
			need_stop = xferflags & I2C_XFER_STOP;

			/* Save read response */
			r[rnext++] = EC_I2C_STATUS_READ;
			r[rnext++] = read_len;
			rnext += read_len;
		} else {
			/* Save write response */
			r[rnext++] = 0;
		}
	}

	/* Unlock port */
	i2c_lock(port, 0);

	/*
	 * Return success even if transfer failed so response is sent.  Host
	 * will check message status to determine the transfer result.
	 */
	args->response_size = rnext;
	return EC_RES_SUCCESS;

 passthru_error:

	/*
	 * If we failed with an error after sending a start bit but before
	 * sending a stop bit, do a dummy read to force a stop bit to be sent.
	 */
	if (need_stop) {
		uint8_t dummy = 0;

		PTHRUPRINTF("[%T i2c passthru forcing stop]\n");
		i2c_xfer(port, slave_addr, 0, 0, &dummy, 1, I2C_XFER_SINGLE);
	}

	/* Unlock port */
	i2c_lock(port, 0);

	return rv;
}
DECLARE_HOST_COMMAND(EC_CMD_I2C_PASSTHRU, i2c_command_passthru, EC_VER_MASK(0));

/*****************************************************************************/
/* Console commands */

static void scan_bus(int port, const char *desc)
{
	int a;
	uint8_t tmp;

	ccprintf("Scanning %d %s", port, desc);

	/* Don't scan a busy port, since reads will just fail / time out */
	a = i2c_get_line_levels(port);
	if (a != I2C_LINE_IDLE) {
		ccprintf(": port busy (SDA=%d, SCL=%d)\n",
			 (a & I2C_LINE_SDA_HIGH) ? 1 : 0,
			 (a & I2C_LINE_SCL_HIGH) ? 1 : 0);
		return;
	}

	i2c_lock(port, 1);

	for (a = 0; a < 0x100; a += 2) {
		watchdog_reload();  /* Otherwise a full scan trips watchdog */
		ccputs(".");

#if defined(CHIP_VARIANT_stm32f100) || defined(CHIP_VARIANT_stm32f10x)
		/*
		 * Hope that address 0 exists, because the i2c_xfer()
		 * implementation on STM32 can't read a byte without writing
		 * one first.
		 *
		 * TODO: remove when that limitation is fixed.
		 */
		tmp = 0;
		if (!i2c_xfer(port, a, &tmp, 1, &tmp, 1, I2C_XFER_SINGLE))
#else
		/* Do a single read */
		if (!i2c_xfer(port, a, NULL, 0, &tmp, 1, I2C_XFER_SINGLE))
#endif
			ccprintf("\n  0x%02x", a);
	}

	i2c_lock(port, 0);
	ccputs("\n");
}

static int command_scan(int argc, char **argv)
{
	int i;

	for (i = 0; i < I2C_PORTS_USED; i++)
		scan_bus(i2c_ports[i].port, i2c_ports[i].name);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(i2cscan, command_scan,
			NULL,
			"Scan I2C ports for devices",
			NULL);
