/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ITE83xx SoC in-system programming tool
 */

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <ftdi.h>
#pragma GCC diagnostic pop

/* default USB device : Servo v2 */
#define SERVO_USB_VID 0x18d1
#define SERVO_USB_PID 0x5002
#define SERVO_INTERFACE INTERFACE_B

/* DBGR I2C addresses */
#define I2C_CMD_ADDR   0x5A
#define I2C_DATA_ADDR  0x35
#define I2C_BLOCK_ADDR 0x79

#define I2C_FREQ 100000

/* I2C pins on the FTDI interface */
#define SCL_BIT        (1 << 0)
#define SDA_BIT        (1 << 1)

/* Chip ID register value */
#define CHIP_ID 0x8380
/* todo : read from CHIPID2 register */
#define FLASH_SIZE (128 * 1024)
#define PAGE_SIZE 256

/* JEDEC SPI Flash commands */
#define SPI_CMD_PAGE_PROGRAM  0x02
#define SPI_CMD_WRITE_DISABLE 0x04
#define SPI_CMD_READ_STATUS   0x05
#define SPI_CMD_WRITE_ENABLE  0x06
#define SPI_CMD_FAST_READ     0x0B
#define SPI_CMD_CHIP_ERASE    0xC7

/* store custom parameters */
const char *input_filename;
const char *output_filename;
static int usb_vid = SERVO_USB_VID;
static int usb_pid = SERVO_USB_PID;
static int usb_interface = SERVO_INTERFACE;
static char *usb_serial;

/* optional command flags */
enum {
	FLAG_UNPROTECT      = 0x01,
	FLAG_ERASE          = 0x02,
};

static int i2c_byte_transfer(struct ftdi_context *ftdi, uint8_t addr,
			     uint8_t *data, int write)
{
	int ret = 0;
	uint8_t ack;
	uint8_t buf[28];
	uint8_t *b = buf;

	/* !!! TODO !!! IMPLEMENT read */
	if (!write)
		return -EINVAL;

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
	*b++ = MPSSE_DO_WRITE | MPSSE_BITMODE | MPSSE_WRITE_NEG;
	*b++ = 0x07; *b++ = (addr << 1) | (write ? 0 : 1);
	/* prepare for ACK */
	*b++ = SET_BITS_LOW; *b++ = 0; *b++ = SCL_BIT;
	/* read ACK */
	*b++ = MPSSE_DO_READ | MPSSE_BITMODE | MPSSE_LSB;
	*b++ = 0;
	*b++ = SEND_IMMEDIATE;
	ret = ftdi_write_data(ftdi, buf, b - buf);
	if (ret < 0) {
		fprintf(stderr, "failed to write address\n");
		goto exit_xfer;
	}
	ret = ftdi_read_data(ftdi, &ack, 1);
	if (ret < 0 || (ack & 0x80) != 0) {
		fprintf(stderr, "Address ACK failed: %d - 0x%02x\n", ret, ack);
		ret = -ENXIO;
		goto exit_xfer;
	}

	b = buf;
	/* write data */
	*b++ = MPSSE_DO_WRITE | MPSSE_BITMODE | MPSSE_WRITE_NEG;
	*b++ = 0x07; *b++ = *data;
	/* prepare for ACK */
	*b++ = SET_BITS_LOW; *b++ = 0; *b++ = SCL_BIT;
	/* read ACK */
	*b++ = MPSSE_DO_READ | MPSSE_BITMODE | MPSSE_LSB;
	*b++ = 0;
	*b++ = SEND_IMMEDIATE;
	ret = ftdi_write_data(ftdi, buf, b - buf);
	if (ret < 0) {
		fprintf(stderr, "failed to write address\n");
		goto exit_xfer;
	}
	ret = ftdi_read_data(ftdi, &ack, 1);
	if (ret < 0 || (ack & 0x80) != 0) {
		fprintf(stderr, "Data ACK failed: %d - 0x%02x\n", ret, ack);
		ret = -ENXIO;
		goto exit_xfer;
	}

exit_xfer:
	b = buf;
	/* STOP condition */
	/* SCL high, SDA low */
	*b++ = SET_BITS_LOW; *b++ = 0; *b++ = SDA_BIT;
	*b++ = SET_BITS_LOW; *b++ = 0; *b++ = SDA_BIT;
	/* SCL high, SDA high */
	*b++ = SET_BITS_LOW; *b++ = 0; *b++ = 0;
	*b++ = SET_BITS_LOW; *b++ = 0; *b++ = 0;

	ret = ftdi_write_data(ftdi, buf, b - buf);
	if (ret < 0)
		fprintf(stderr, "failed to write %d bytes\n", (int)(b - buf));
	return ret;
}

static int i2c_write_byte(struct ftdi_context *ftdi, uint8_t cmd, uint8_t data)
{
	int reta, retd;

	reta = i2c_byte_transfer(ftdi, I2C_CMD_ADDR, &cmd, 1);
	retd = i2c_byte_transfer(ftdi, I2C_DATA_ADDR, &data, 1);

	return reta || retd ? -EIO : 0;
}

static int i2c_read_byte(struct ftdi_context *ftdi, uint8_t cmd, uint8_t *data)
{
	int reta, retd;

	reta = i2c_byte_transfer(ftdi, I2C_CMD_ADDR, &cmd, 1);
	retd = i2c_byte_transfer(ftdi, I2C_DATA_ADDR, data, 1);

	return reta || retd ? -EIO : 0;
}

static int check_communication(struct ftdi_context *ftdi)
{
	int ret;
	uint8_t id1 = 0xff;

	ret = i2c_read_byte(ftdi, 0x00, &id1);
	if (ret < 0)
		return ret;
	printf("ID1 0x%02x\n", id1);

	return 0;
}

/* SPI Flash generic command */
static int spi_flash_command(struct ftdi_context *ftdi, uint8_t cmd)
{
	int ret = 0;

	ret |= i2c_write_byte(ftdi, 0x07, 0xff);
	ret |= i2c_write_byte(ftdi, 0x06, 0xff);
	ret |= i2c_write_byte(ftdi, 0x05, 0xfe);
	ret |= i2c_write_byte(ftdi, 0x04, 0x00);
	ret |= i2c_write_byte(ftdi, 0x08, 0x00);
	ret |= i2c_write_byte(ftdi, 0x05, 0xfd);
	ret |= i2c_write_byte(ftdi, 0x08, cmd);

	return ret ? -EIO : 0;
}

/* Poll SPI Flash Read Status register until BUSY is reset */
static int spi_poll_busy(struct ftdi_context *ftdi)
{
	uint8_t reg = 0xff;

	while (1) {
		int ret = spi_flash_command(ftdi, SPI_CMD_READ_STATUS);
		if (ret < 0)
			return ret;

		ret = i2c_read_byte(ftdi, 0x08, &reg);
		if (ret < 0)
			return ret;
		if ((ret & 0x01) == 0)
			break;
	}
	return 0;
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

	wave = malloc(SPECIAL_BUFFER_SIZE);

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

	/* wait for PLL stable for 5ms (plus remaining USB transfers) */
	usleep(10 * MSEC);

special_failed:
	free(wave);
	return ret;
}

static int windex;
static const char wheel[] = {'|', '/', '-', '\\' };
static void draw_spinner(uint32_t remaining, uint32_t size)
{
	int percent = (size - remaining)*100/size;
	printf("\r%c%3d%%", wheel[windex++], percent);
	windex %= sizeof(wheel);
}

int command_read_pages(struct ftdi_context *ftdi, uint32_t address,
		       uint32_t size, uint8_t *buffer)
{
	int res;
	uint32_t remaining = size;
	uint8_t cnt;
	uint16_t page;

	res = spi_flash_command(ftdi, SPI_CMD_WRITE_ENABLE);
	if (res < 0) {
		fprintf(stderr, "Flash write enable FAILED (%d)\n", res);
		goto failed_read;
	}

	while (remaining) {
		uint8_t cmd = 0x9;
		int i;

		cnt = (remaining > PAGE_SIZE) ? PAGE_SIZE : remaining;
		page = address % PAGE_SIZE;

		draw_spinner(remaining, size);
		/* Fast Read command */
		res = spi_flash_command(ftdi, SPI_CMD_FAST_READ);
		if (res < 0)
			goto failed_read;
		res = i2c_write_byte(ftdi, 0x08, page >> 8);
		res += i2c_write_byte(ftdi, 0x08, page & 0xff);
		res += i2c_write_byte(ftdi, 0x08, 0x00);
		res += i2c_write_byte(ftdi, 0x08, 0x00);
		if (res < 0) {
			fprintf(stderr, "page address set failed\n");
			goto failed_read;
		}
		/* read page data */
		res = i2c_byte_transfer(ftdi, I2C_CMD_ADDR, &cmd, 1);
		for (i = 0; i < cnt; i++, buffer++) {
			res = i2c_byte_transfer(ftdi, I2C_DATA_ADDR, buffer, 0);
			if (res < 0) {
				fprintf(stderr, "page data read failed\n");
				goto failed_read;
			}
		}

		address += cnt;
		remaining -= cnt;
	}
	res = size;

failed_read:
	if (spi_flash_command(ftdi, SPI_CMD_WRITE_DISABLE) < 0)
		fprintf(stderr, "Flash write disable FAILED\n");

	return res;
}

int command_write_pages(struct ftdi_context *ftdi, uint32_t address,
			uint32_t size, uint8_t *buffer)
{
	int res;
	uint32_t remaining = size;
	uint32_t cnt;
	uint16_t page;

	res = spi_flash_command(ftdi, SPI_CMD_WRITE_ENABLE);
	if (res < 0) {
		fprintf(stderr, "Flash write enable FAILED (%d)\n", res);
		goto failed_write;
	}

	while (remaining) {
		uint8_t cmd = 0xA;
		int i;

		cnt = (remaining > PAGE_SIZE) ? PAGE_SIZE : remaining;
		page = address % PAGE_SIZE;

		draw_spinner(remaining, size);

		/* Program Page command */
		res = spi_flash_command(ftdi, SPI_CMD_PAGE_PROGRAM);
		if (res < 0)
			goto failed_write;
		/* send page address */
		res = i2c_write_byte(ftdi, 0x08, page >> 8);
		res = i2c_write_byte(ftdi, 0x08, page & 0xff);
		res = i2c_write_byte(ftdi, 0x08, 0x00);
		if (res < 0) {
			fprintf(stderr, "page address set failed\n");
			goto failed_write;
		}
		/* write page data */
		res = i2c_byte_transfer(ftdi, I2C_CMD_ADDR, &cmd, 1);
		for (i = 0; i < cnt; i++, buffer++) {
			res = i2c_byte_transfer(ftdi, I2C_DATA_ADDR, buffer, 1);
			if (res < 0) {
				fprintf(stderr, "page data write failed\n");
				goto failed_write;
			}
		}

		address += cnt;
		remaining -= cnt;
	}
	res = size;

failed_write:
	if (spi_flash_command(ftdi, SPI_CMD_WRITE_DISABLE) < 0)
		fprintf(stderr, "Flash write disable FAILED\n");

	return res;
}

int command_write_unprotect(struct ftdi_context *ftdi)
{
	/* TODO(http://crosbug.com/p/): implement me ? */
	return 0;
}

int command_erase(struct ftdi_context *ftdi, uint32_t len, uint32_t off)
{
	int res;

	if (off != 0 || len != FLASH_SIZE) {
		fprintf(stderr, "Only full chip erase is supported\n");
		return -EINVAL;
	}

	res = spi_flash_command(ftdi, SPI_CMD_WRITE_ENABLE);
	if (res < 0) {
		fprintf(stderr, "Flash write enable FAILED (%d)\n", res);
		goto failed_erase;
	}

	res = spi_flash_command(ftdi, SPI_CMD_CHIP_ERASE);
	if (res < 0)
		fprintf(stderr, "Flash chip erase FAILED (%d)\n", res);
	res = spi_poll_busy(ftdi);
	if (res < 0)
		fprintf(stderr, "Flash BUSY polling FAILED (%d)\n", res);

failed_erase:
	if (spi_flash_command(ftdi, SPI_CMD_WRITE_DISABLE) < 0)
		fprintf(stderr, "Flash write disable FAILED\n");

	return res;
}

/* Return zero on success, a negative error value on failures. */
int read_flash(struct ftdi_context *ftdi, const char *filename,
	       uint32_t offset, uint32_t size)
{
	int res;
	FILE *hnd;
	uint8_t *buffer = malloc(size);

	if (!buffer) {
		fprintf(stderr, "Cannot allocate %d bytes\n", size);
		return -ENOMEM;
	}

	hnd = fopen(filename, "w");
	if (!hnd) {
		fprintf(stderr, "Cannot open file %s for writing\n", filename);
		free(buffer);
		return -EIO;
	}

	if (!size)
		size = FLASH_SIZE;
	printf("Reading %d bytes at 0x%08x\n", size, offset);
	res = command_read_pages(ftdi, offset, size, buffer);
	if (res > 0) {
		if (fwrite(buffer, res, 1, hnd) != 1)
			fprintf(stderr, "Cannot write %s\n", filename);
	}
	printf("\r   %d bytes read.\n", res);

	fclose(hnd);
	free(buffer);
	return (res < 0) ? res : 0;
}

/* Return zero on success, a negative error value on failures. */
int write_flash(struct ftdi_context *ftdi, const char *filename,
		uint32_t offset)
{
	int res, written;
	FILE *hnd;
	int size = FLASH_SIZE;
	uint8_t *buffer = malloc(size);

	if (!buffer) {
		fprintf(stderr, "Cannot allocate %d bytes\n", size);
		return -ENOMEM;
	}

	hnd = fopen(filename, "r");
	if (!hnd) {
		fprintf(stderr, "Cannot open file %s for reading\n", filename);
		free(buffer);
		return -EIO;
	}
	if ((res = fread(buffer, 1, size, hnd)) <= 0) {
		fprintf(stderr, "Cannot read %s\n", filename);
		free(buffer);
		return -EIO;
	}
	fclose(hnd);

	printf("Writing %d bytes at 0x%08x\n", res, offset);
	written = command_write_pages(ftdi, offset, res, buffer);
	if (written != res) {
		fprintf(stderr, "Error writing to flash\n");
		free(buffer);
		return -EIO;
	}
	printf("\rDone.\n");

	free(buffer);
	return 0;
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

static const struct option longopts[] = {
	{"product", 1, 0, 'p'},
	{"vendor", 1, 0, 'v'},
	{"interface", 1, 0, 'i'},
	{"serial", 1, 0, 's'},
	{"read", 1, 0, 'r'},
	{"write", 1, 0, 'w'},
	{"erase", 0, 0, 'e'},
	{"help", 0, 0, 'h'},
	{"unprotect", 0, 0, 'u'},
	{NULL, 0, 0, 0}
};

void display_usage(char *program)
{
	fprintf(stderr, "Usage: %s [-d <tty>] [-b <baudrate>] [-u] [-e] [-U]"
		" [-r <file>] [-w <file>]\n", program);
	fprintf(stderr, "--v[endor] <0x1234> : USB vendor ID\n");
	fprintf(stderr, "--p[roduct] <0x1234> : USB product ID\n");
	fprintf(stderr, "--s[erial] <serialname> : USB serial name\n");
	fprintf(stderr, "--i[interface] <1> : FTDI interface: A=1, B=2, ...\n");
	fprintf(stderr, "--b[audrate] <baudrate> : set serial port speed "
			"to <baudrate> bauds\n");

	fprintf(stderr, "--u[nprotect] : remove flash write protect\n");
	fprintf(stderr, "--e[rase] : erase all the flash content\n");
	fprintf(stderr, "--r[ead] <file> : read the flash content and "
			"write it into <file>\n");
	fprintf(stderr, "--w[rite] <file> : read <file> and "
			"write it to flash\n");

	exit(2);
}

int parse_parameters(int argc, char **argv)
{
	int opt, idx;
	int flags = 0;

	while ((opt = getopt_long(argc, argv, "v:p:i:s:ehr:w:u?",
				  longopts, &idx)) != -1) {
		switch (opt) {
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
		case 'e':
			flags |= FLAG_ERASE;
			break;
		case 'h':
		case '?':
			display_usage(argv[0]);
			break;
		case 'r':
			input_filename = optarg;
			break;
		case 'w':
			output_filename = optarg;
			break;
		case 'u':
			flags |= FLAG_UNPROTECT;
			break;
		}
	}
	return flags;
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

	if (check_communication(hnd) < 0)
		goto terminate;

	if (flags & FLAG_UNPROTECT)
		command_write_unprotect(hnd);

	if (flags & FLAG_ERASE || output_filename)
		command_erase(hnd, FLASH_SIZE, 0);

	if (input_filename) {
		ret = read_flash(hnd, input_filename, 0, FLASH_SIZE);
		if (ret)
			goto terminate;
	}

	if (output_filename) {
		ret = write_flash(hnd, output_filename, 0);
		if (ret)
			goto terminate;
	}

	/* Normal exit */
	ret = 0;
terminate:
	/* Close the FTDI USB handle */
	ftdi_usb_close(hnd);
	ftdi_free(hnd);
	return ret;
}
