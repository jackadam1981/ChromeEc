/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* SPI module for Chrome EC */

#include "clock.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "registers.h"
#include "spi.h"
#include "task.h"
#include "timer.h"
#include "util.h"


/* Console output macros */
#define CPUTS(outstr) cputs(CC_SPI, outstr)
#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)

enum sspi_clk_sel {
	sspi_clk_24mhz = 0,
	sspi_clk_12mhz,
	sspi_clk_8mhz,
	sspi_clk_6mhz,
	sspi_clk_4p8mhz,
	sspi_clk_4mhz,
	sspi_clk_3p428mhz,
	sspi_clk_3mhz,
};

enum sspi_ch_sel {
	SSPI_CH_CS0 = 0,
	SSPI_CH_CS1,
};

static void sspi_frequency(enum sspi_clk_sel freq)
{
	/*
	 * bit[6:5]
	 * Bit 6:Clock Polarity (CLPOL)
	 * 0: SSCK is low in the idle mode.
	 * 1: SSCK is high in the idle mode.
	 * Bit 5:Clock Phase (CLPHS)
	 * 0: Latch data on the first SSCK edge.
	 * 1: Latch data on the second SSCK edge.
	 *
	 * bit[4:2]
	 * 000b: 1/2 clk_sspi
	 * 001b: 1/4 clk_sspi
	 * 010b: 1/6 clk_sspi
	 * 011b: 1/8 clk_sspi
	 * 100b: 1/10 clk_sspi
	 * 101b: 1/12 clk_sspi
	 * 110b: 1/14 clk_sspi
	 * 111b: 1/16 clk_sspi
	 *
	 * SSCK frequency is [freq] MHz and mode 3.
	 * note, clk_sspi need equal to 48MHz above.
	 */
	IT83XX_SSPI_SPICTRL1 |= (0x20 | (freq << 2));
}

static void sspi_transmission_end(void)
{
	/* Write 1 to end the SPI transmission. */
	IT83XX_SSPI_SPISTS = 0x20;

	/* Short delay for "Transfer End Flag" */
	IT83XX_GCTRL_WNCKR = 0;

	/* Write 1 to clear this bit and terminate data transmission. */
	IT83XX_SSPI_SPISTS = 0x02;
}

/* We assume only one SPI port in the chip, one SPI device */
int spi_enable(int port, int enable)
{
	if (enable) {
		/*
		 * bit[5:4]
		 * 00b: SPI channel 0 and channel 1 are disabled.
		 * 10b: SSCK/SMOSI/SMISO/SSCE1# are enabled.
		 * 01b: SSCK/SMOSI/SMISO/SSCE0# are enabled.
		 * 11b: SSCK/SMOSI/SMISO/SSCE1#/SSCE0# are enabled.
		 */
		if (port == SSPI_CH_CS1)
			IT83XX_GPIO_GRC1 |= 0x20;
		else
			IT83XX_GPIO_GRC1 |= 0x10;

		gpio_config_module(MODULE_SPI_MASTER, 1);
	} else {
		if (port == SSPI_CH_CS1)
			IT83XX_GPIO_GRC1 &= ~0x20;
		else
			IT83XX_GPIO_GRC1 &= ~0x10;

		gpio_config_module(MODULE_SPI_MASTER, 0);
	}

	return EC_SUCCESS;
}

int spi_transaction(const struct spi_device_t *spi_device,
		const uint8_t *txdata, int txlen,
		uint8_t *rxdata, int rxlen)
{
	int idx;
	uint8_t port = spi_device->port;
	static struct mutex spi_mutex;

	mutex_lock(&spi_mutex);
	/* bit[0]: Write cycle */
	IT83XX_SSPI_SPICTRL2 &= ~0x04;
	for (idx = 0x00; idx < txlen; idx++) {
		IT83XX_SSPI_SPIDATA = txdata[idx];
		if (port == SSPI_CH_CS1)
			/* Write 1 to start the data transmission of CS1 */
			IT83XX_SSPI_SPISTS |= 0x08;
		else
			/* Write 1 to start the data transmission of CS0 */
			IT83XX_SSPI_SPISTS |= 0x10;
	}

	/* bit[1]: Read cycle */
	IT83XX_SSPI_SPICTRL2 |= 0x04;
	for (idx = 0x00; idx < rxlen; idx++) {
		if (port == SSPI_CH_CS1)
			/* Write 1 to start the data transmission of CS1 */
			IT83XX_SSPI_SPISTS |= 0x08;
		else
			/* Write 1 to start the data transmission of CS0 */
			IT83XX_SSPI_SPISTS |= 0x10;
		rxdata[idx] = IT83XX_SSPI_SPIDATA;
		if (rxdata[idx] == 0xed) {
			//ccprints("[SSPI] hello cmd done addr:%pP", &rxdata[idx]);
			break;
		}
	}

	sspi_transmission_end();
	mutex_unlock(&spi_mutex);

	return EC_SUCCESS;
}

static void sspi_init(void)
{
	int i;

	clock_enable_peripheral(CGC_OFFSET_SSPI, 0, 0);
	sspi_frequency(sspi_clk_6mhz);

	/*
	 * bit[5:3] Byte Width (BYTEWIDTH)
	 * 000b: 8-bit transmission
	 * 001b: 1-bit transmission
	 * 010b: 2-bit transmission
	 * 011b: 3-bit transmission
	 * 100b: 4-bit transmission
	 * 101b: 5-bit transmission
	 * 110b: 6-bit transmission
	 * 111b: 7-bit transmission
	 *
	 * bit[1] Blocking selection
	 */
	IT83XX_SSPI_SPICTRL2 |= 0x02;

	for (i = 0; i < spi_devices_used; i++)
		/* Disabling spi module */
		spi_enable(spi_devices[i].port, 0);
}
DECLARE_HOOK(HOOK_INIT, sspi_init, HOOK_PRIO_INIT_SPI);


static uint8_t rx_fifo[0x1000];

static int command_spislv_hello(int argc, char **argv)
{
	int i;
	uint8_t tx_buf[] = {0x03, 0xEE, 0x01, 0x00, 0x00,
		0x00, 0x04, 0x00, 0x01, 0x02, 0x03, 0x04};

	ccprints("[SSPI] rx_fifo=%pP", rx_fifo);

	for (i = 0; i < 1; i++) {
		/* Enable spi module */
		spi_enable(spi_devices[i].port, 1);
		spi_transaction(&spi_devices[i], tx_buf,
			12, rx_fifo, 0x1000);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(spihello, command_spislv_hello,
			"spihello",
			"SPI hello cmmand");

#define EC_CMD_WRITE 0x0014

struct ec_params_wr {
	uint8_t *in_data;
} __ec_align4;

struct ec_response_rd {
	uint8_t out_data[240];
} __ec_align4;

static enum ec_status command_write(struct host_cmd_handler_args *args)
{
	//const uint8_t *p = args->params;
	struct ec_response_rd *r = args->response;
	int i;

	for(i = 0; i < 240; i++)
		r->out_data[i] = 0x0a;

	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_WRITE,
		     command_write,
		     EC_VER_MASK(0));

static int command_spislv_write(int argc, char **argv)
{
	int i, j;
	uint8_t tx_buf[128] = {0x03, 0x15, 0x14, 0x00, 0x00, 0x00, 0x78, 0x00};

	ccprints("[SSPI] rx_fifo=%pP", rx_fifo);

	for (j = 8; j < 128; j++)
		tx_buf[j] = j-7;

	for (i = 0; i < 1; i++) {
		/* Enable spi module */
		spi_enable(spi_devices[i].port, 1);
		spi_transaction(&spi_devices[i], tx_buf,
			128, rx_fifo, 0x1000);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(spiwr128, command_spislv_write,
			"spiwr128",
			"SPI wr128 cmmand");


static int command_spi_elm_hc(int argc, char **argv)
{
	int i;

	uint8_t tx_buf[][24] = {
		{0x03, 0x1a, 0x87, 0x00, 0x00, 0x00, 0x04, 0x00,
		 0x20, 0x26, 0x12, 0x00},
		{0x03, 0x93, 0x87, 0x00, 0x00, 0x00, 0x04, 0x00,
		 0x60, 0x1e, 0x21, 0x40},
		{0x03, 0xf2, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x03, 0xfb, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x03, 0x2a, 0x8f, 0x00, 0x00, 0x00, 0x04, 0x00,
		 0x00, 0x40, 0x00, 0x00},
		{0x03, 0xe6, 0xa0, 0x00, 0x00, 0x00, 0x09, 0x00,
		 0x01, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68},
		//{0x03,0x42,0xb6,0x00,0x00,0x00,0x04,0x00,0x01,0x00,0x00,0x00},
		{0x03, 0xfb, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x03, 0xf2, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x03, 0xb2, 0x0b, 0x40, 0x00, 0x00, 0x00, 0x00},
		{0x03, 0x8d, 0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x67},
		//{0x03,0x96,0x67,0x00,0x00,0x00,0x00,0x00},
		{0x03, 0xfd, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x03, 0xc3, 0x2b, 0x00, 0x02, 0x00, 0x0d, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x03, 0xc8, 0x28, 0x00, 0x00, 0x00, 0x01, 0x00, 0x0c},
		{0x03, 0xe9, 0x12, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00},
		{0x03, 0x2a, 0xd3, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x03, 0x5b, 0x08, 0x00, 0x00, 0x00, 0x02, 0x00, 0x98, 0x00},
		{0x03, 0x66, 0x97, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x03, 0x62, 0x98, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01},
		//{0x03,0x74,0x9E,0x00,0x00,0x00,0x00,0x0b,0x00,0x01,0x02,0x2c,0x00
		// 0x01,0x00,0x2c,0x80,0x04,0x00,0x00},
		//{0x03,0x57,0x15,0x00,0x01,0x00,0x08,0x00,0x44,0x00,0x00,0x00,
		// 0x44,0x00,0x00,0x00},
	};

	ccprints("[SSPI] rx_fifo=%pP", rx_fifo);

	/* Enable spi module */
	spi_enable(spi_devices[0].port, 1);

	for (i = 0; i < sizeof(tx_buf)/sizeof(tx_buf[0]); i++) {

		spi_transaction(&spi_devices[0], tx_buf[i],
			24, rx_fifo, 0x1000);
		usleep(50*MSEC);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(spielm, command_spi_elm_hc,
			"spielm",
			"SPI elm host cmmand");
