/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ITE83xx SoC SMB communication common code
 */

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <ftdi.h>
#pragma GCC diagnostic pop

#include "itecommon.h"

/* default USB device : Servo v2 */
#define SERVO_USB_VID 0x18d1
#define SERVO_USB_PID 0x5002
#define SERVO_INTERFACE INTERFACE_B

#define I2C_FREQ 150000

/* I2C pins on the FTDI interface */
#define SCL_BIT        (1 << 0)
#define SDA_BIT        (1 << 1)

/* Chip ID register value */
#define CHIP_ID 0x8380

/* Size for FTDI outgoing buffer */
#define FTDI_CMD_BUF_SIZE (1<<12)

/* store custom parameters */
static int usb_vid = SERVO_USB_VID;
static int usb_pid = SERVO_USB_PID;
static int usb_interface = SERVO_INTERFACE;
static char *usb_serial;

/* debug traces : default OFF*/
static int debug;

/* number of bytes to send consecutively before checking for ACKs */
#define TX_BUFFER_LIMIT	32

int i2c_add_send_byte(void *ctxt, uint8_t *buf, uint8_t *ptr,
		      uint8_t *tbuf, int tcnt)
{
	struct ftdi_context *ftdi = ctxt;
	int ret, i, j;
	int tx_buffered = 0;
	static uint8_t ack[TX_BUFFER_LIMIT];
	uint8_t *b = ptr;
	uint8_t failed_ack = 0;

	for (i = 0; i < tcnt; i++) {
		/* WORKAROUND: force SDA before sending the next byte */
		*b++ = SET_BITS_LOW; *b++ = SDA_BIT; *b++ = SCL_BIT | SDA_BIT;
		/* write byte */
		*b++ = MPSSE_DO_WRITE | MPSSE_BITMODE | MPSSE_WRITE_NEG;
		*b++ = 0x07; *b++ = *tbuf++;
		/* prepare for ACK */
		*b++ = SET_BITS_LOW; *b++ = 0; *b++ = SCL_BIT;
		/* read ACK */
		*b++ = MPSSE_DO_READ | MPSSE_BITMODE | MPSSE_LSB;
		*b++ = 0;
		*b++ = SEND_IMMEDIATE;

		tx_buffered++;

		/*
		 * On the last byte, or every TX_BUFFER_LIMIT bytes, read the
		 * ACK bits.
		 */
		if (i == tcnt-1 || (tx_buffered == TX_BUFFER_LIMIT)) {
			/* write data */
			ret = ftdi_write_data(ftdi, buf, b - buf);
			if (ret < 0) {
				fprintf(stderr, "failed to write byte\n");
				return ret;
			}

			/* read ACK bits */
			ret = ftdi_read_data(ftdi, &ack[0], tx_buffered);
			for (j = 0; j < tx_buffered; j++) {
				if ((ack[j] & 0x80) != 0)
					failed_ack = ack[j];
			}

			/* check ACK bits */
			if (ret < 0 || failed_ack) {
				if (debug)
					fprintf(stderr,
						"write ACK fail: %d, 0x%02x\n",
						ret, failed_ack);
				return  -ENXIO;
			}

			/* reset for next set of transactions */
			b = ptr;
			tx_buffered = 0;
		}
	}
	return 0;
}

int i2c_add_recv_bytes(void *ctxt, uint8_t *buf, uint8_t *ptr,
		       uint8_t *rbuf, int rcnt)
{
	struct ftdi_context *ftdi = ctxt;
	int ret, i;
	uint8_t *b = ptr;

	for (i = 0; i < rcnt; i++) {
		/* set SCL low */
		*b++ = SET_BITS_LOW; *b++ = 0; *b++ = SCL_BIT;
		/* read the byte on the wire */
		*b++ = MPSSE_DO_READ; *b++ = 0; *b++ = 0;

		if (i == rcnt - 1) {
			/* NACK last byte */
			*b++ = SET_BITS_LOW; *b++ = 0; *b++ = SCL_BIT;
			*b++ = MPSSE_DO_WRITE | MPSSE_BITMODE | MPSSE_WRITE_NEG;
			*b++ = 0; *b++ = 0xff; *b++ = SEND_IMMEDIATE;
		} else {
			/* ACK all other bytes */
			*b++ = SET_BITS_LOW; *b++ = 0; *b++ = SCL_BIT | SDA_BIT;
			*b++ = MPSSE_DO_WRITE | MPSSE_BITMODE | MPSSE_WRITE_NEG;
			*b++ = 0; *b++ = 0; *b++ = SEND_IMMEDIATE;
		}
	}

	ret = ftdi_write_data(ftdi, buf, b - buf);
	if (ret < 0) {
		fprintf(stderr, "failed to prepare read\n");
		return ret;
	}
	ret = ftdi_read_data(ftdi, rbuf, rcnt);
	if (ret < 0)
		fprintf(stderr, "read byte failed\n");
	return ret;
}

int i2c_byte_transfer(void *ctxt, uint8_t addr, uint8_t *data,
		      int write, int numbytes)
{
	struct ftdi_context *ftdi = ctxt;
	int ret = 0, rets;
	static uint8_t buf[FTDI_CMD_BUF_SIZE];
	uint8_t *b = buf;
	uint8_t slave_addr;

	/* START condition */
	/* SCL & SDA high */
	*b++ = SET_BITS_LOW; *b++ = 0; *b++ = 0;
	*b++ = SET_BITS_LOW; *b++ = 0; *b++ = 0;
	/* SCL high, SDA low */
	*b++ = SET_BITS_LOW; *b++ = 0; *b++ = SDA_BIT;
	*b++ = SET_BITS_LOW; *b++ = 0; *b++ = SDA_BIT;
	/* SCL low, SDA low */
	*b++ = SET_BITS_LOW; *b++ = 0; *b++ = SCL_BIT | SDA_BIT;
	*b++ = SET_BITS_LOW; *b++ = 0; *b++ = SCL_BIT | SDA_BIT;

	/* send address */
	slave_addr = (addr << 1) | (write ? 0 : 1);
	ret = i2c_add_send_byte(ftdi, buf, b, &slave_addr, 1);
	if (ret < 0) {
		if (debug)
			fprintf(stderr, "address %02x failed\n", addr);
		ret = -ENXIO;
		goto exit_xfer;
	}

	b = buf;
	if (write) /* write data */
		ret = i2c_add_send_byte(ftdi, buf, b, data, numbytes);
	else /* read data */
		ret = i2c_add_recv_bytes(ftdi, buf, b, data, numbytes);

exit_xfer:
	b = buf;
	/* STOP condition */
	/* SCL high, SDA low */
	*b++ = SET_BITS_LOW; *b++ = 0; *b++ = SDA_BIT;
	*b++ = SET_BITS_LOW; *b++ = 0; *b++ = SDA_BIT;
	/* SCL high, SDA high */
	*b++ = SET_BITS_LOW; *b++ = 0; *b++ = 0;
	*b++ = SET_BITS_LOW; *b++ = 0; *b++ = 0;

	rets = ftdi_write_data(ftdi, buf, b - buf);
	if (rets < 0)
		fprintf(stderr, "failed to send STOP\n");
	return ret;
}

int i2c_write_byte(void *ctxt, uint8_t cmd, uint8_t data)
{
	struct ftdi_context *ftdi = ctxt;
	int ret;

	ret = i2c_byte_transfer(ftdi, I2C_CMD_ADDR, &cmd, 1, 1);
	if (ret < 0)
		return -EIO;
	ret = i2c_byte_transfer(ftdi, I2C_DATA_ADDR, &data, 1, 1);
	if (ret < 0)
		return -EIO;

	return 0;
}

int i2c_read_byte(void *ctxt, uint8_t cmd, uint8_t *data)
{
	struct ftdi_context *ftdi = ctxt;
	int ret;

	ret = i2c_byte_transfer(ftdi, I2C_CMD_ADDR, &cmd, 1, 1);
	if (ret < 0)
		return -EIO;
	ret = i2c_byte_transfer(ftdi, I2C_DATA_ADDR, data, 0, 1);
	if (ret < 0)
		return -EIO;

	return 0;
}

int check_chipid(void *ctxt)
{
	struct ftdi_context *ftdi = ctxt;
	int ret;
	int flash_size;
	uint8_t ver = 0xff;
	uint16_t id = 0xffff;

	ret = i2c_read_byte(ftdi, 0x00, (uint8_t *)&id + 1);
	if (ret < 0)
		return ret;
	ret = i2c_read_byte(ftdi, 0x01, (uint8_t *)&id);
	if (ret < 0)
		return ret;
	ret = i2c_read_byte(ftdi, 0x02, &ver);
	if (ret < 0)
		return ret;
	if (id != CHIP_ID) {
		fprintf(stderr, "Invalid chip id: %04x\n", id);
		return -EINVAL;
	}
	/* compute embedded flash size from CHIPVER field */
	flash_size = (128 + (ver & 0xF0)) * 1024;

	printf("CHIPID %04x, CHIPVER %02x, Flash size %d kB\n", id, ver,
			flash_size / 1024);

	return (id << 8) | ver;
}

static int config_i2c(struct ftdi_context *ftdi)
{
	int ret;
	uint8_t buf[5];
	uint16_t divisor;

	ret = ftdi_set_latency_timer(ftdi, 16 /* ms */);
	if (ret < 0)
		fprintf(stderr, "Cannot set latency\n");

	ret = ftdi_set_bitmode(ftdi, 0, BITMODE_RESET);
	if (ret < 0) {
		fprintf(stderr, "Cannot reset MPSSE\n");
		return -EIO;
	}
	ret = ftdi_set_bitmode(ftdi, 0, BITMODE_MPSSE);
	if (ret < 0) {
		fprintf(stderr, "Cannot enable MPSSE\n");
		return -EIO;
	}

	ret = ftdi_usb_purge_buffers(ftdi);
	if (ret < 0)
		fprintf(stderr, "Cannot purge buffers\n");

	/* configure the clock */
	divisor = (60000000 / (2 * I2C_FREQ * 3 / 2 /* 3-phase CLK */) - 1);
	buf[0] = EN_3_PHASE;
	buf[1] = DIS_DIV_5;
	buf[2] = TCK_DIVISOR;
	buf[3] = divisor & 0xff;
	buf[4] = divisor >> 8;
	ret = ftdi_write_data(ftdi, buf, sizeof(buf));
	return ret;
}

/* Special waveform definition */
#define SPECIAL_LEN_USEC  50000ULL /* us */
#define SPECIAL_FREQ     400000ULL

#define SPECIAL_PATTERN 0x0000020301010302ULL

#define MSEC    1000
#define USEC 1000000

#define SPECIAL_BUFFER_SIZE \
	(((SPECIAL_LEN_USEC * SPECIAL_FREQ * 2 / USEC) + 7) & ~7)

static int send_special_waveform(struct ftdi_context *ftdi)
{
	int ret;
	int i;
	uint64_t *wave;
	uint8_t release_lines[] = {SET_BITS_LOW, 0, 0};

	wave = malloc(SPECIAL_BUFFER_SIZE);

	printf("Waiting for the EC power-on sequence ...");
	fflush(stdout);

retry:
	/* Reset the FTDI into a known state */
	ret = ftdi_set_bitmode(ftdi, 0xFF, BITMODE_RESET);
	if (ret != 0) {
		fprintf(stderr, "failed to reset FTDI\n");
		goto special_failed;
	}

	/*
	 * set the clock divider,
	 * so we output a new bitbang value every 2.5us.
	 */
	ret = ftdi_set_baudrate(ftdi, 160000);
	if (ret != 0) {
		fprintf(stderr, "failed to set bitbang clock\n");
		goto special_failed;
	}

	/* Enable asynchronous bit-bang mode */
	ret = ftdi_set_bitmode(ftdi, 0xFF, BITMODE_BITBANG);
	if (ret != 0) {
		fprintf(stderr, "failed to set bitbang mode\n");
		goto special_failed;
	}

	/* fill the buffer with the waveform pattern */
	for (i = 0; i < SPECIAL_BUFFER_SIZE / sizeof(uint64_t); i++)
		wave[i] = SPECIAL_PATTERN;

	ret = ftdi_write_data(ftdi, (uint8_t *)wave, SPECIAL_BUFFER_SIZE);
	if (ret < 0)
		fprintf(stderr, "Cannot output special waveform\n");

	/* clean everything to go back to regular I2C communication */
	ftdi_usb_purge_buffers(ftdi);
	ftdi_set_bitmode(ftdi, 0xff, BITMODE_RESET);
	config_i2c(ftdi);
	ftdi_write_data(ftdi, release_lines, sizeof(release_lines));

	/* wait for PLL stable for 5ms (plus remaining USB transfers) */
	usleep(10 * MSEC);

	/* if we cannot communicate, retry the sequence */
	if (check_chipid(ftdi) < 0)
		goto retry;

special_failed:
	printf("Done.\n");
	free(wave);
	return ret;
}

static struct ftdi_context *open_ftdi_device(int vid, int pid,
					     int interface, char *serial)
{
	struct ftdi_context *ftdi;
	int ret;

	ftdi = ftdi_new();
	if (!ftdi) {
		fprintf(stderr, "Cannot allocate context memory\n");
		return NULL;
	}

	ret = ftdi_set_interface(ftdi, interface);
	if (ret < 0) {
		fprintf(stderr, "cannot set ftdi interface %d: %s(%d)\n",
			interface, ftdi_get_error_string(ftdi), ret);
		goto open_failed;
	}
	ret = ftdi_usb_open_desc(ftdi, vid, pid, NULL, serial);
	if (ret < 0) {
		fprintf(stderr, "unable to open ftdi device: %s(%d)\n",
			ftdi_get_error_string(ftdi), ret);
		goto open_failed;
	}
	return ftdi;

open_failed:
	ftdi_free(ftdi);
	return NULL;
}

void display_common_usage(void)
{
	fprintf(stderr, "--d[ebug] : output debug traces\n");
	fprintf(stderr, "--v[endor] <0x1234> : USB vendor ID\n");
	fprintf(stderr, "--p[roduct] <0x1234> : USB product ID\n");
	fprintf(stderr, "--s[erial] <serialname> : USB serial string\n");
	fprintf(stderr, "--i[interface] <1> : FTDI interface: A=1, B=2, ...\n");
}

int parse_common_arg(int opt, char *program)
{
	switch (opt) {
	case 'd':
		debug = 1;
		break;
	case 'v':
		usb_vid = strtol(optarg, NULL, 16);
		break;
	case 'p':
		usb_pid = strtol(optarg, NULL, 16);
		break;
	case 'i':
		usb_interface = atoi(optarg);
		break;
	case 's':
		usb_serial = optarg;
		break;
	case 'h':
	case '?':
		display_usage(program);
		break;
	default:
		/* not a common argument */
		return 0;
	}
	return 1;
}

int main(int argc, char **argv)
{
	void *hnd;
	int ret = 1;
	int flags;

	/* Parse command line options */
	flags = parse_parameters(argc, argv);

	/* Open the USB device */
	hnd = open_ftdi_device(usb_vid, usb_pid, usb_interface, usb_serial);
	if (hnd == NULL)
		return 1;

	/* Trigger embedded monitor detection */
	if (send_special_waveform(hnd) < 0)
		goto terminate;

	if (config_i2c(hnd) < 0)
		goto terminate;

	/* tool specific implementation */
	ret = tool_main(hnd, flags);

terminate:
	/* Close the FTDI USB handle */
	ftdi_usb_close(hnd);
	ftdi_free(hnd);
	return ret;
}
