/* Copyright 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Console output module for Chrome EC */

#include "console.h"
#include "uart.h"
#include "usb_console.h"
#include "util.h"
/***********************************************************/
/***********************************************************/
/***********************************************************/
#include <errno.h>
#include <zephyr.h>
#include <sys/printk.h>
#include <device.h>
#include <drivers/spi.h>
/***********************************************************/
/***********************************************************/
/***********************************************************/
#ifdef CONFIG_CONSOLE_CHANNEL
/* Default to all channels active */
#ifndef CC_DEFAULT
#define CC_DEFAULT CC_ALL
#endif
static uint32_t channel_mask = CC_DEFAULT;
static uint32_t channel_mask_saved = CC_DEFAULT;

/*
 * List of channel names;
 *
 * We could try to get clever with #ifdefs or board-specific lists of channel
 * names, so that for example boards without port80 support don't waste binary
 * size on the channel name string for "port80".  Pruning the channel list
 * might also become more important if we have >32 channels - for example, if
 * we decide to replace enum console_channel with enum module_id.
 */
static const char * const channel_names[] = {
	#define CONSOLE_CHANNEL(enumeration, string) string,
	#include "console_channel.inc"
	#undef CONSOLE_CHANNEL
};
BUILD_ASSERT(ARRAY_SIZE(channel_names) == CC_CHANNEL_COUNT);
/* ensure that we are not silently masking additional channels */
BUILD_ASSERT(CC_CHANNEL_COUNT <= 8*sizeof(uint32_t));
/***********************************************************/
/***********************************************************/
/***********************************************************/


/* Console commands & test codes of IT8XXX2 SPI */
static const struct device *spi_dev;

static const struct spi_config spi_cfg = {
	.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB |
		     SPI_MODE_CPOL | SPI_MODE_CPHA,
	.frequency = 4000000,
	.slave = 0,
};

static void spi_init(void)
{
	printk("%s\r\n", __func__);
	spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi0));

	if (!device_is_ready(spi_dev)) {
		printk("[user] Err: SPI device is not ready\r\n");
	}
	else
		printk("[user] It seems that the %s is ready.\r\n", spi_dev->name);

}

void spi_we(void)
{
	int err;
	static uint8_t tx_buffer[1];
	uint8_t cnt;

	const struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = sizeof(tx_buffer)
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

	struct spi_buf rx_buf = {
		.buf = NULL,
		.len = 0
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 0
	};

	tx_buffer[0] = 0x06;

	printk("[user] %s\r\n", __func__);
	printk("[user] len txbuf=%d\r\n", tx_buf.len);
	printk("[user] len tx_buffer=%d\r\n", sizeof(tx_buffer));

	for(cnt=0 ; cnt < tx_buf.len ; cnt++) {
		printk("[user] tx_buffer[%d]=0x%X\r\n", cnt, tx_buffer[cnt]);
	}

	err = spi_transceive(spi_dev, &spi_cfg, &tx, &rx);

	if (err)
		printk("SPI error: %d\n", err);

}


void spi_a3(void)
{
	int err;
	static uint8_t tx_buffer[1];
	uint8_t cnt;

	const struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = sizeof(tx_buffer)
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

	struct spi_buf rx_buf = {
		.buf = NULL,
		.len = 0
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 0
	};

	tx_buffer[0] = 0xA3;

	printk("[user] %s\r\n", __func__);
	printk("[user] len txbuf=%d\r\n", tx_buf.len);
	printk("[user] len tx_buffer=%d\r\n", sizeof(tx_buffer));

	for(cnt=0 ; cnt < tx_buf.len ; cnt++) {
		printk("[user] tx_buffer[%d]=0x%X\r\n", cnt, tx_buffer[cnt]);
	}

	err = spi_transceive(spi_dev, &spi_cfg, &tx, &rx);

	if (err)
		printk("SPI error: %d\n", err);

}


void spi_9f(void)
{
	int err;
	static uint8_t tx_buffer[1];
	static uint8_t rx_buffer[3];
	uint8_t cnt;

	const struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = sizeof(tx_buffer)
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

	struct spi_buf rx_buf = {
		.buf = NULL,
		.len = 0
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 0
	};

	printk("[user] %s\r\n", __func__);
	tx_buffer[0] = 0x9F;

	printk("[user] len txbuf=%d\r\n", tx_buf.len);
	printk("[user] len tx_buffer=%d\r\n", sizeof(tx_buffer));

	for(cnt=0 ; cnt < tx_buf.len ; cnt++) {
		printk("[user] tx_buffer[%d]=0x%X\r\n", cnt, tx_buffer[cnt]);
	}

	err = spi_transceive(spi_dev, &spi_cfg, &tx, &rx);

	if (err) {
		printk("SPI error: %d\n", err);
	} else {
		/* Connect MISO to MOSI for loopback */
		printk("TX sent: %x\n", tx_buffer[0]);
		printk("RX recv: %x\n", rx_buffer[0]);
		printk("RX recv: %x\n", rx_buffer[1]);
		printk("RX recv: %x\n", rx_buffer[2]);
		tx_buffer[0]++;
	}


}


void spi_read_id(void)
{
	int err;
	static uint8_t tx_buffer[1];
	static uint8_t rx_buffer[3];
	uint8_t cnt;

	const struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = sizeof(tx_buffer)
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

	struct spi_buf rx_buf = {
		.buf = rx_buffer,
		.len = sizeof(rx_buffer),
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 1
	};

	printk("[user] %s\r\n", __func__);
	tx_buffer[0] = 0x9F;

	printk("[user] len txbuf=%d\r\n", tx_buf.len);
	printk("[user] len tx_buffer=%d\r\n", sizeof(tx_buffer));

	for(cnt=0 ; cnt < tx_buf.len ; cnt++) {
		printk("[user] tx_buffer[%d]=0x%X\r\n", cnt, tx_buffer[cnt]);
	}

	err = spi_transceive(spi_dev, &spi_cfg, &tx, &rx);
	if (err) {
		printk("SPI error: %d\n", err);
	} else {
		/* Connect MISO to MOSI for loopback */
		printk("TX sent: %x\n", tx_buffer[0]);
		printk("RX recv: %x\n", rx_buffer[0]);
		printk("RX recv: %x\n", rx_buffer[1]);
		printk("RX recv: %x\n", rx_buffer[2]);
		tx_buffer[0]++;
	}
}

int spi_cmd_we(void)
{
	printk("SPI WE\n");

	spi_init();
	spi_we();

	return 0;
}

int spi_cmd_a3(void)
{
	printk("SPI A3h\n");

	spi_init();
	spi_a3();

	return 0;
}

int spi_cmd_9f(void)
{
	printk("SPI A3h\n");

	spi_init();
	spi_9f();

	return 0;
}

int spi_cmd_read_id(void)
{
	printk("SPI Read ID\n");

	spi_init();
	spi_read_id();

	return 0;
}

static int command_spi_cmd(int argc, char **argv)
{
	int ret=0;
	char *e;
	int opt;

	printk("\r\n");
	printk("SPI Command Tests\r\n");
	printk("%s\r\n", __func__);

	if (argc > 2)
		return EC_ERROR_PARAM_COUNT;

	if (argc == 2) {

		opt = strtoi(argv[1], &e, 0);

		switch(opt) {

			case 0:
					ret = spi_cmd_we();
					break;
			case 1:
					ret = spi_cmd_a3();
					break;
			case 2:
					ret = spi_cmd_9f();
					break;
			case 3:
					ret = spi_cmd_read_id();
					break;
			default:
					ret = spi_cmd_a3();
					break;
		}
	}
	else {
		ret = spi_cmd_a3();
	}

	return ret;

}
DECLARE_CONSOLE_COMMAND(spi_test, command_spi_cmd,
			NULL,
			"SPI Test");
/* SPI Commands End Up Here */

/***********************************************************/
/***********************************************************/
/***********************************************************/
static int console_channel_name_to_index(const char *name)
{
	int i;

	for (i = 0; i < CC_CHANNEL_COUNT; i++) {
		if (!strncasecmp(name, channel_names[i], strlen(name)))
			return i;
	}

	/* Not found */
	return -1;
}

void console_channel_enable(const char *name)
{
	int index = console_channel_name_to_index(name);

	if (index >= 0 && index != CC_COMMAND)
		channel_mask |= CC_MASK(index);
}
void console_channel_disable(const char *name)
{
	int index = console_channel_name_to_index(name);

	if (index >= 0 && index != CC_COMMAND)
		channel_mask &= ~CC_MASK(index);
}

bool console_channel_is_disabled(enum console_channel channel)
{
	if (!(CC_MASK(channel) & channel_mask))
		return true;
	return false;
}
#endif /* CONFIG_CONSOLE_CHANNEL */

#ifndef CONFIG_ZEPHYR
/*****************************************************************************/
/* Channel-based console output */

int cputs(enum console_channel channel, const char *outstr)
{
	int rv1, rv2;

	/* Filter out inactive channels */
	if (console_channel_is_disabled(channel))
		return EC_SUCCESS;

	rv1 = usb_puts(outstr);
	rv2 = uart_puts(outstr);

	return rv1 == EC_SUCCESS ? rv2 : rv1;
}

int cprintf(enum console_channel channel, const char *format, ...)
{
	int rv1, rv2;
	va_list args;

	/* Filter out inactive channels */
	if (console_channel_is_disabled(channel))
		return EC_SUCCESS;

	usb_va_start(args, format);
	rv1 = usb_vprintf(format, args);
	usb_va_end(args);

	va_start(args, format);
	rv2 = uart_vprintf(format, args);
	va_end(args);

	return rv1 == EC_SUCCESS ? rv2 : rv1;
}

int cprints(enum console_channel channel, const char *format, ...)
{
	int r, rv;
	va_list args;

	/* Filter out inactive channels */
	if (console_channel_is_disabled(channel))
		return EC_SUCCESS;

	rv = cprintf(channel, "[%pT ", PRINTF_TIMESTAMP_NOW);

	va_start(args, format);
	r = uart_vprintf(format, args);
	if (r)
		rv = r;
	va_end(args);

	usb_va_start(args, format);
	r = usb_vprintf(format, args);
	if (r)
		rv = r;
	usb_va_end(args);

	r = cputs(channel, "]\n");
	return r ? r : rv;
}
#endif /* CONFIG_ZEPHYR */

void cflush(void)
{
	uart_flush_output();
}

/*****************************************************************************/
/* Console commands */

#ifdef CONFIG_CONSOLE_CHANNEL
/* Set active channels */
static int command_ch(int argc, char **argv)
{
	int i;
	char *e;

	/* If one arg, save / restore, or set the mask */
	if (argc == 2) {
		if (strcasecmp(argv[1], "save") == 0) {
			channel_mask_saved = channel_mask;
			return EC_SUCCESS;
		} else if (strcasecmp(argv[1], "restore") == 0) {
			channel_mask = channel_mask_saved;
			return EC_SUCCESS;

		} else {
			/* Set the mask */
			int m = strtoi(argv[1], &e, 0);
			if (*e)
				return EC_ERROR_PARAM1;

			/* No disabling the command output channel */
			channel_mask = m | CC_MASK(CC_COMMAND);

			return EC_SUCCESS;
		}
	}

	/* Print the list of channels */
	ccputs(" # Mask     E Channel\n");
	for (i = 0; i < CC_CHANNEL_COUNT; i++) {
		ccprintf("%2d %08x %c %s\n",
			 i, CC_MASK(i),
			 (channel_mask & CC_MASK(i)) ? '*' : ' ',
			 channel_names[i]);
		cflush();
	}
	return EC_SUCCESS;
};
DECLARE_SAFE_CONSOLE_COMMAND(chan, command_ch,
			     "[ save | restore | <mask> ]",
			     "Save, restore, get or set console channel mask");
#endif /* CONFIG_CONSOLE_CHANNEL */
