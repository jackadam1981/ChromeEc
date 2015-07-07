/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "blob.h"
#include "gpio.h"
#include "hooks.h"
#include "queue.h"
#include "registers.h"
#include "spi.h"
#include "sps.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "pmu.h"
#include "watchdog.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_SPI, outstr)
#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)

/*
 * SPI Packet Size: 128-Bytes
 */
#define SPI_PACKET_SIZE  128

#define BIT_ORDER_MSB_FIRST 1
#define BIT_ORDER_LSB_FIRST 0

#define SPI_COMMAND_RESPONSE_BUFFER_SIZE 1

/* SPI Flash Max transfer size */
#define GC_SPI_FLASH_MAX_XFER_SIZE       (1<<7)

/* SPI Flash Max transfer mask */
#define GC_SPI_FLASH_MAX_XFER_MASK       (GC_SPI_FLASH_MAX_XFER_SIZE - 1)

/* SPI submission request */
struct spi_submission_request {
	const void *data;
	uint16_t len;
};

/* SPI response status */
struct spi_response_status {
	const void *data;
	uint16_t len;
	uint16_t tx_count;
	uint16_t tx_err_count;
};

static struct queue const g_spi_tx_submission_queue =
	QUEUE_NULL(SPI_COMMAND_RESPONSE_BUFFER_SIZE,
			struct spi_submission_request);

static struct queue const g_spi_tx_completion_queue =
	QUEUE_NULL(SPI_COMMAND_RESPONSE_BUFFER_SIZE,
			struct spi_response_status);

static struct spi_response_status spi_resp;

#define __SPI_SPS_LOOPBACK_TEST__
#ifdef __SPI_SPS_LOOPBACK_TEST__

static uint32_t g_tx_data[SPI_PACKET_SIZE>>2];
static uint32_t g_rx_data[SPI_PACKET_SIZE>>2];
#endif

/*
 * Set SPI transaction length
 * This is useful for performing reads without having to
 * pre-fill the TX buffers. Len should be <= GC_SPI_FLASH_MAX_XFER_SIZE.
 */
static void spi_length_set(uint32_t port, uint32_t len)
{
	len -= 1;
	len &= GC_SPI_FLASH_MAX_XFER_MASK;
	GWRITE_FIELD_I(SPI, port, XACT, SIZE, len);
}

/*
 * Queue data to the SPI TX Submission buffer
 * @param data Pointer to data
 * @param len Length of data
 * @return return number of bytes write into spi tx queue.
 */
static void *spi_memcpy4(void *_dest, const void *_src, size_t len)
{
	int i;
	volatile uint32_t *dest = (volatile uint32_t *) _dest;
	volatile uint32_t *src  = (volatile uint32_t *) _src;
	len >>= 2;
	for (i = 0; i < len; i++)
		dest[i] = src[i];
	CPRINTS("memcpy4:%08X %d", src[0], len);
	return _dest;
}

static int spi_write(uint32_t port,
		const uint32_t *data, uint32_t len)
{
	volatile uint32_t *dest;
	struct spi_submission_request req;
	req.data = data;
	req.len  = len;

	if (len > SPI_PACKET_SIZE)
		return 0;

	if (queue_space(&g_spi_tx_submission_queue) == 0)
		return 0;

	len = QUEUE_ADD_UNITS(&g_spi_tx_submission_queue, &req, sizeof(req));

	spi_length_set(port, SPI_PACKET_SIZE);

	dest = GREG32_ADDR_I(SPI, port, TX_DATA);
	spi_memcpy4((void *)dest, data, SPI_PACKET_SIZE);

	GWRITE_FIELD_I(SPI, port, ISTATE_CLR, TXDONE, 1);
	GWRITE_FIELD_I(SPI, port, XACT, START, 1);
	return len;
}

/*
 * Read data from the SPI RX buffer
 * RevA1: Requried input data is 4-byte aligned; len is multiple of 4
 * @param data Pointer to 32-bit data
 * @param len Length of data
 */
static int spi_read32(uint32_t port, uint32_t *data, uint32_t len)
{
	int i;
	volatile uint32_t *src;

	if (len & 0x3)
		return EC_ERROR_PARAM1;

	src = GREG32_ADDR_I(SPI, port, RX_DATA);
	len >>= 2;
	for (i = 0; i < len; i++)
		data[i] = src[i];

	return EC_SUCCESS;
}

/*
 * Configure SPI data transmission format
 */
void spi_configure(enum spi_clock_mode clk_mode)
{
	/* Configure SPI master */
	GWRITE_FIELD(SPI, CTRL, CPHA, clk_mode & 1);
	GWRITE_FIELD(SPI, CTRL, CPOL, (clk_mode >> 1) & 1);

	/* [5:2] CSB to SCK setup time in SCK cycles + 1.5 */
	GWRITE_FIELD(SPI, CTRL, CSBSU, 1);
	/* [9:6] CSB from SCK hold time in SCK cycles + 1 */
	GWRITE_FIELD(SPI, CTRL, CSBHLD, 1);

	/* [21:10] SPI clk divider */
	GWRITE_FIELD(SPI, CTRL, IDIV,    7);

	GWRITE_FIELD(SPI, CTRL, TXBITOR, BIT_ORDER_MSB_FIRST);
	GWRITE_FIELD(SPI, CTRL, RXBITOR, BIT_ORDER_MSB_FIRST);

	/*Tx Interrupt enable */
	GREG32(SPI, ICTRL) = 1;
}


/*
 * TBD: Tx Interrupt enable
 *  GREG32(SPI, ICTRL) = 1;  enable tx_done interrupt
 *  GREG32(SPI, ISTATE) == 1; check if tx_done
 *  GREG32(SPI, ISTATE_CLR) = 1; write 1 to clear tx_done
 */
int spi_enable(int enable)
{
	static uint8_t enable_flag;

	if (enable == enable_flag)
		return EC_SUCCESS;

	if (enable)
		spi_configure(0);

	enable_flag = enable;
	return EC_SUCCESS;
}


int spi_transaction128(const uint32_t *txdata, int txlen,
		    uint32_t *rxdata, int rxlen)
{
	int num = 0, rc = EC_SUCCESS;

	uint32_t port = 0;
	struct spi_response_status resp;

	if (txdata && (txlen > 0) && !(txlen & 0x3))
		num = spi_write(port, txdata, txlen);

	if (num == 0)
		return rc;

	if (rxdata && (rxlen > 0) && !(rxlen & 0x3)) {
		while (queue_count(&g_spi_tx_completion_queue) == 0) {
			usleep(10);
			watchdog_reload();
		}
		QUEUE_REMOVE_UNITS(&g_spi_tx_completion_queue,
			&resp, sizeof(resp));

		CPRINTS("tx:%p num:%d cnt:%d %d resp:%p",
			txdata, num, resp.tx_count,
			resp.tx_err_count, resp.data);
		spi_read32(port, rxdata, rxlen);
	}

	return EC_SUCCESS;
}

static void spi_init(void)
{
	/* init clock */
	pmu_clock_en(PERIPH_SPI);

	spi_enable(1);

	task_enable_irq(GC_IRQNUM_SPI0_SPITXINT);
}
DECLARE_HOOK(HOOK_INIT, spi_init, HOOK_PRIO_DEFAULT);

static void spi_tx_interrupt(int port)
{
	int num;
	struct spi_submission_request req;

	num = QUEUE_REMOVE_UNITS(&g_spi_tx_submission_queue,
		&req, sizeof(req));
	if (num == 0)
		return;

	spi_resp.tx_count++;
	spi_resp.data = req.data;
	spi_resp.len = req.len;
	QUEUE_ADD_UNITS(&g_spi_tx_completion_queue,
		&spi_resp, sizeof(spi_resp));

	if (queue_count(&g_spi_tx_submission_queue) == 0) {
		GWRITE_FIELD_I(SPI, port, ISTATE_CLR, TXDONE, 1);
		return;
	}
}

void _spi0_tx_interrupt(void)
{
	spi_tx_interrupt(0);
}
DECLARE_IRQ(GC_IRQNUM_SPI0_SPITXINT, _spi0_tx_interrupt, 1);

#ifdef __SPI_SPS_LOOPBACK_TEST__

#define SPI_TEST_DATA_ADDRESS_MODE 0xAD

static uint16_t loopback_pkt_cnt;
void sps_loopback(uint8_t *data,
		size_t data_size, int cs_status)
{
	static uint8_t buf[1024];
	uint8_t *bufptr = buf;

	if (!data_size)
		return;

	memcpy(bufptr, data, data_size);
	loopback_pkt_cnt++;
	while (data_size) {
		size_t cnt = sps_transmit(bufptr, data_size);
		data_size -= cnt;
		bufptr += cnt;
	}
}


static uint16_t loopback_pkt_seq;
static int spi_loopback_test(int val, int num)
{
	int rc = 0, i, c;
	uint8_t tx_len = SPI_PACKET_SIZE;
	uint8_t rx_len = SPI_PACKET_SIZE;

	CPRINTS("Loopback Test: num=%d (0x%p 0x%p)", num, g_tx_data, g_rx_data);
	CPRINTS("spi_resp.tx_count:%d", spi_resp.tx_count);
	CPRINTS("spi_resp.tx_err_count:%d", spi_resp.tx_err_count);
#ifdef __TBD_FIXED_SPS_REGISTER_FUNC__
	sps_unregister_rx_handler();
	sps_register_rx_handler(sps_loopback);
#endif

	for (i = 0; i < num; i++) {

		CPRINTS("Test:%d", i);
		for (c = 0; c < SPI_PACKET_SIZE/4; c++) {
			g_tx_data[c] = (val == SPI_TEST_DATA_ADDRESS_MODE) ?
					(val + c) : val;
			g_rx_data[c] = 0;
		}
		g_tx_data[0] = loopback_pkt_seq++;
		g_tx_data[0] |= ((SPI_PACKET_SIZE - 2) << 16);

		CPRINTS("xfer:%d %d", tx_len, rx_len);
		rc = spi_transaction128(g_tx_data, tx_len, g_rx_data, rx_len);
		if (rc) {
			CPRINTS("spi transaction error; rc:%d", rc);
			cflush();
			break;
		}

		rc = 0;
		if ((g_tx_data[0] - 1) != g_rx_data[0])
			rc++;

		for (c = 1; c < SPI_PACKET_SIZE/4; c++)
			if (g_tx_data[c] != g_rx_data[c])
				rc++;
		if (rc)
			break;
	}
	if (!rc)
		CPRINTS("SPI->SPS Loopback Test Passed (%d).",
				loopback_pkt_cnt);
	else {
		CPRINTS("Loopback Test Failed (%d).", loopback_pkt_cnt);
		for (i = 0; i < SPI_PACKET_SIZE/4; i++)
			CPRINTS("%08X %08X", g_tx_data[i], g_rx_data[i]);
	}
#ifdef __TBD_FIXED_SPS_REGISTER_FUNC__
	sps_unregister_rx_handler();
#endif
	return rc;
}


static int command_spitest(int argc, char **argv)
{
	char *e;
	uint32_t seed = SPI_TEST_DATA_ADDRESS_MODE, num = 0;

	if (argc < 1)
		return EC_ERROR_PARAM_COUNT;

	if (argc >= 2)
		num = strtoi(argv[1], &e, 0);

	if (argc >= 3)
		seed = strtoi(argv[2], &e, 0);

	spi_loopback_test(seed, num);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(spitest, command_spitest,
			"[num] [seed]",
			"num: loop count",
			NULL);

#endif
