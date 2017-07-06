/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* QMSPI master module for MEC1701 */

#include "common.h"
#include "console.h"
#include "dma.h"
#include "gpio.h"
#include "registers.h"
#include "spi.h"
#include "timer.h"
#include "util.h"
#include "hooks.h"
#include "task.h"
#include "dma_chip.h"
#include "spi_chip.h"
#include "qmspi_chip.h"

#define CPUTS(outstr) cputs(CC_SPI, outstr)
#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)

#define QMSPI_TRANSFER_TIMEOUT (100 * MSEC)
#define QMSPI_BYTE_TRANSFER_TIMEOUT_US (3 * MSEC)
#define QMSPI_BYTE_TRANSFER_POLL_INTERVAL_US 20



#ifdef LFW
/*
 * MEC17xx 32-bit timer 0 configured for 1us count down mode and no
 * interrupt in the LFW environment. Don't need to sleep CPU in LFW.
 */
static int qmspi_wait(uint32_t mask, uint32_t mval)
{
	uint32_t t1, t2, td;

	t1 = MEC17XX_TMR32_CNT(0);

	while ((MEC17XX_QMSPI0_STS & mask) != mval) {
		t2 = MEC17XX_TMR32_CNT(0);
		if (t1 >= t2)
			td = t1 - t2;
		else
			td = t1 + (0xfffffffful - t2);
		if (td > QMSPI_BYTE_TRANSFER_TIMEOUT_US)
			return EC_ERROR_TIMEOUT;
	}
	return EC_SUCCESS;
}
#else
/*
 * This version uses the full EC_RO/RW timer infrastructure and it needs
 * a timer ISR to handle timer underflow. Without the ISR we observe false
 * timeouts when debugging with JTAG.
 * QMSPI_BYTE_TRANSFER_TIMEOUT_US currently 3ms
 * QMSPI_BYTE_TRANSFER_POLL_INTERVAL_US currently 100 us
 */
static int qmspi_wait(uint32_t mask, uint32_t mval)
{
	timestamp_t deadline;

	deadline.val = get_time().val + (QMSPI_BYTE_TRANSFER_TIMEOUT_US);

	while ((MEC17XX_QMSPI0_STS & mask) != mval) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;

		usleep(QMSPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}
	return EC_SUCCESS;
}
#endif /* #ifdef LFW */

/*
 * Wait for QMSPI read using DMA to finish.
 * DMA subsystem has 100 ms timeout
 */
int qmspi_transaction_wait(const struct spi_device_t *spi_device)
{
	const struct dma_option *opdma;

	opdma = spi_dma_option(spi_device, SPI_DMA_OPTION_RD);
	if (opdma != NULL)
		return dma_wait(opdma->channel);

	return EC_ERROR_INVAL;
}

/*
 * Create QMSPI transmit data descriptor not using DMA.
 * Transmit on MOSI pin (single/full-duplex) from TX FIFO.
 * TX FIFO filled by CPU.
 * Caller will apply close and last flags if applicable.
 */
static uint32_t qmspi_build_tx_descr(uint32_t ntx, uint32_t ndid)
{
	uint32_t d;

	d = MEC17XX_QMSPI_CTRL_1X + MEC17XX_QMSPI_CTRL_TX_DATA;
	d |= ((ndid & 0x0F) << MEC17XX_QMSPI_CTRL_NEXT_DESCR_BITPOS);

	if (ntx <= MEC17XX_QMSPI_CTRL_MAX_UNITS)
		d |= MEC17XX_QMSPI_CTRL_XFRU_1B;
	else {
		if ((ntx & 0x0f) == 0) {
			ntx >>= 4;
			d |= MEC17XX_QMSPI_CTRL_XFRU_16B;
		} else if ((ntx & 0x03) == 0) {
			ntx >>= 2;
			d |= MEC17XX_QMSPI_CTRL_XFRU_4B;
		} else
			d |= MEC17XX_QMSPI_CTRL_XFRU_1B;

		if (ntx > MEC17XX_QMSPI_CTRL_MAX_UNITS)
			return 0; /* overflow unit count field */
	}

	d |= (ntx << MEC17XX_QMSPI_CTRL_NUM_UNITS_BITPOS);

	return d;
}

/*
 * Create QMSPI receive data descriptor using DMA.
 * Receive data on MISO pin (single/full-duplex) and store in QMSPI
 * RX FIFO. QMSPI triggers DMA channel to read from RX FIFO and write
 * to memory. Return value is an uint64_t where low 32-bit word is the
 * descriptor and upper 32-bit word is DMA channel unit length with
 * value (1, 2, or 4).
 * Caller will apply close and last flags if applicable.
 */
static uint64_t qmspi_build_rx_descr(uint32_t raddr,
		uint32_t nrx, uint32_t ndid)
{
	uint32_t d, dmau, na;
	uint64_t u;

	d = MEC17XX_QMSPI_CTRL_1X + MEC17XX_QMSPI_CTRL_RX_EN;
	d |= ((ndid & 0x0F) << MEC17XX_QMSPI_CTRL_NEXT_DESCR_BITPOS);

	dmau = 1;
	na = (raddr | nrx) & 0x03;
	if (na == 0) {
		d |= MEC17XX_QMSPI_CTRL_RX_DMA_4B;
		dmau <<= 2;
	} else if (na == 0x02) {
		d |= MEC17XX_QMSPI_CTRL_RX_DMA_2B;
		dmau <<= 1;
	} else {
		d |= MEC17XX_QMSPI_CTRL_RX_DMA_1B;
	}

	if ((nrx & 0x0f) == 0) {
		nrx >>= 4;
		d |= MEC17XX_QMSPI_CTRL_XFRU_16B;
	} else if ((nrx & 0x03) == 0) {
		nrx >>= 2;
		d |= MEC17XX_QMSPI_CTRL_XFRU_4B;
	} else {
		d |= MEC17XX_QMSPI_CTRL_XFRU_1B;
	}

	u = 0;
	if (nrx <= MEC17XX_QMSPI_CTRL_MAX_UNITS) {
		d |= (nrx << MEC17XX_QMSPI_CTRL_NUM_UNITS_BITPOS);
		u = dmau;
		u <<= 32;
		u |= d;
	}

	return u;
}

/*
 * QMSPI controller must control chip select therefore this routine
 * configures QMSPI to assert SPI CS# and de-assert when done.
 * Because we are paranoid and want robust code, always soft-reset QMSPI
 * and re-configure before starting a transaction.
 * transmit using CPU and QMSPI TX FIFO(no DMA).
 * Transmit and receive byte lengths are limited as follows:
 * length is odd, limited to 0x7FFF bytes
 * length is a multiple of 4, limited to 0x1FFFC bytes
 * length is a multiple of 16, limited to 0x7FFF0 bytes
 * Receive using QMSPI RX FIFO and DMA.
 * NOTE: This routine only handle SPI flash commands not requiring
 * dummy clocks. QMSPI can support SPI flash commands with dummy clocks
 * and mode bytes by configuring extra descriptors.
 * For example, a full SPI Flash command implementation will require
 * additional information:
 * 1. number of pins for transmitting command byte
 * 2. number of pins for transmitting address and length of address.
 * 3. Optional mode byte plus info to transmit mode byte as a byte
 *    or break mode byte up into 4-bit fields. First field is
 *    transmitted as data, second field tri-states I/O pins.
 * 4. number of dummy clocks after address or optional mode byte.
 *    Dummy clocks are transmitted with I/O pins tri-stated and
 *    configured for receive.
 * 4. number of pins for reading data.
 * Descriptor 0 = transmit command plus 3 or 4 address bytes
 * Descriptor 1 = dummy clocks at 1X: configure number of units = 1,
 *   TX data disabled, and do not write any data to TX FIFO. QMSPI will
 *   output clocks only and tri-state I/O lines.
 * Optional mode byte where bits[7:4]=data and bits[3:0]=tri-state pins
 * Descriptor 2 = transmit data in bit mode, 4 units(bits), and write byte
 *   with bits[7:4]=data to TX FIFO
 * Descriptor 3 = transmit no data in bit mode, 4 units(bits),
 *   do not write any data to TX FIFO.
 * Descriptor 4 = RX DMA and configure DMA channel for RX.
 */
int qmspi_transaction_async(const struct spi_device_t *spi_device,
				const uint8_t *txdata, int txlen,
				uint8_t *rxdata, int rxlen)
{
	const struct dma_option *opdma;
	uint32_t d, did, dmau;
	uint64_t u;

	if (spi_device == NULL)
		return EC_ERROR_PARAM1;

	/* soft reset the controller */
	MEC17XX_QMSPI0_MODE_ACT_SRST = MEC17XX_QMSPI_M_SOFT_RESET;
	d = spi_device->div;
	d <<= MEC17XX_QMSPI_M_CLKDIV_BITPOS;
	d += (MEC17XX_QMSPI_M_ACTIVATE + MEC17XX_QMSPI_M_SPI_MODE0);
	MEC17XX_QMSPI0_MODE = d;
	MEC17XX_QMSPI0_CTRL = MEC17XX_QMSPI_CTRL_DESCR_MODE_EN;

	d = did = 0;

	if (txlen > 0) {
		if (txdata == NULL)
			return EC_ERROR_PARAM2;

		d = qmspi_build_tx_descr((uint32_t)txlen, 1);
		if (d == 0) /* txlen too large */
			return EC_ERROR_OVERFLOW;

		MEC17XX_QMSPI0_DESCR(did) = d;
	}

	if (rxlen > 0) {
		if (rxdata == NULL)
			return EC_ERROR_PARAM4;

		u = qmspi_build_rx_descr((uint32_t)rxdata,
				(uint32_t)rxlen, 2);

		d = (uint32_t)u;
		dmau = u >> 32;

		if (txlen > 0)
			did++;
		MEC17XX_QMSPI0_DESCR(did) = d;

		opdma = spi_dma_option(spi_device, SPI_DMA_OPTION_RD);
		dma_xfr_start_rx(opdma, dmau, (uint32_t)rxlen, rxdata);
	}

	MEC17XX_QMSPI0_DESCR(did) |= (MEC17XX_QMSPI_CTRL_CLOSE +
			MEC17XX_QMSPI_CTRL_DESCR_LAST);

	MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_START;

	while (txlen--) {
		if (MEC17XX_QMSPI0_STS & MEC17XX_QMSPI_STS_TX_BUFF_FULL) {
			if (qmspi_wait(MEC17XX_QMSPI_STS_TX_BUFF_EMPTY,
					MEC17XX_QMSPI_STS_TX_BUFF_EMPTY) !=
							EC_SUCCESS) {
				MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_STOP;
				return EC_ERROR_TIMEOUT;
			}
		} else
			MEC17XX_QMSPI0_TX_FIFO8 = *txdata++;
	}

	return EC_SUCCESS;
}

/*
 * Wait for QMSPI descriptor mode transfer to finish.
 * QMSPI is configured to perform a complete transaction.
 * Assert CS#
 * optional transmit
 * 	CPU keeps filling TX FIFO until all bytes are transmitted.
 * optional receive
 * 	QMSPI is configured to read rxlen bytes and uses a DMA channel
 * 	to move data from its RX FIFO to memory.
 * De-assert CS#
 * This routine can be called with QMSPI hardware in four states:
 * 1. Transmit only and QMSPI has finished (empty TX FIFO) by the time
 *    this routine is called. QMSPI.Status transfer done status will be
 *    set and QMSPI HW has de-asserted SPI CS#.
 * 2. Transmit only and QMSPI TX FIFO is still transmitting.
 *    QMSPI transfer done status is not asserted and CS# is still
 *    asserted. QMSPI HW will de-assert CS# when done or firmware
 *    manually stops QMSPI.
 * 3. Receive was enabled and DMA channel is moving data from
 *    QMSPI RX FIFO to memory. QMSPI.Status transfer done and DMA done
 *    status bits are not set. QMSPI SPI CS# will stay asserted until
 *    transaction finishes or firmware manually stops QMSPI.
 * 4. Receive was enabled and DMA channel is finished. QMSPI RX FIFO
 *    should be empty and DMA channel is done.  QMSPI.Status transfer
 *    done and DMA done status bits will be set. QMSPI HW has de-asserted
 *    SPI CS#.
 * We are using QMSPI in descriptor mode. The definition of QMSPI.Status
 * transfer complete bit in this mode is: complete will be set to 1 only
 * when the last buffer completes its transfer.
 * TX only sets complete when transfer unit count is matched and all units
 * have been clocked out of the TX FIFO.
 * RX DMA transfer complete will be set when the last transfer unit
 * is out of the RX FIFO but DMA may not be complete until it finishes
 * moving the transfer unit to memory.
 * If TX only spin on QMSPI.Status Transfer_Complete bit.
 * If RX used spin on QMsPI.Status Transfer_Complete and DMA_Complete.
 * Search descriptors looking for RX DMA enabled.
 * If RX DMA is enabled add DMA complete flag to status mask.
 * Spin while QMSPI.Status & mask != mask or timeout.
 * If timeout force QMSPI to stop and exit spin loop.
 * if DMA was enabled disable DMA channel.
 * Clear QMSPI.Status and FIFO's
 */
int qmspi_transaction_flush(const struct spi_device_t *spi_device)
{
	int ret;
	uint32_t did, mask;
	const struct dma_option *opdma;
	timestamp_t deadline;

	if (spi_device == NULL)
		return EC_ERROR_PARAM1;

	mask = MEC17XX_QMSPI_STS_DONE;
	did = 0;
	while (did < MEC17XX_QMSPI_MAX_DESCR) {
		if (MEC17XX_QMSPI0_DESCR(did) &
				MEC17XX_QMSPI_CTRL_RX_DMA_MASK) {
			mask |= MEC17XX_QMSPI_STS_DMA_DONE;
			break;
		}
		did++;
	}
	if (did == MEC17XX_QMSPI_MAX_DESCR)
		return EC_ERROR_UNKNOWN;

	ret = EC_SUCCESS;
	deadline.val = get_time().val + QMSPI_TRANSFER_TIMEOUT;

	while ((MEC17XX_QMSPI0_STS & mask) != mask) {
		if (timestamp_expired(deadline, NULL)) {
			MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_STOP;
			ret = EC_ERROR_TIMEOUT;
			break;
		}
		usleep(QMSPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}

	if (mask & MEC17XX_QMSPI_STS_DMA_DONE) {
		opdma = spi_dma_option(spi_device, SPI_DMA_OPTION_RD);
		if (opdma == NULL)
			return EC_ERROR_INVAL;

		dma_disable(opdma->channel);
		dma_clear_isr(opdma->channel);
	}

	/* clear QMSPI FIFO's */
	MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_CLR_FIFOS;
	MEC17XX_QMSPI0_STS = 0xffffffff;

	return ret;
}

/**
 * Enable QMSPI controller and MODULE_SPI_FLASH pins.
 *
 * @param hw_port b[3:0]=0 and b[7:4]=0
 * @param enable
 * @return EC_SUCCESS or EC_ERROR_INVAL if port is unrecognized
 * @note called by spi_enable in mec1701/spi.c
 *
 */
int qmspi_enable(int hw_port, int enable)
{
	uint8_t dummy __attribute__((unused)) = 0;

	trace2(0, QMSPI, 0, "qmspi_enable: port = %d enable = %d",
			hw_port, enable);

	if (hw_port != QMSPI0_PORT)
		return EC_ERROR_INVAL;

	gpio_config_module(MODULE_SPI_FLASH, (enable > 0));

	if (enable) {
		MEC17XX_PCR_SLP_DIS_DEV(MEC17XX_PCR_QMSPI);
		MEC17XX_QMSPI0_MODE_ACT_SRST = MEC17XX_QMSPI_M_SOFT_RESET;
		dummy = MEC17XX_QMSPI0_MODE_ACT_SRST;
		MEC17XX_QMSPI0_MODE = (MEC17XX_QMSPI_M_ACTIVATE +
				MEC17XX_QMSPI_M_SPI_MODE0 +
				MEC17XX_QMSPI_M_CLKDIV_12M);
	} else {
		MEC17XX_QMSPI0_MODE_ACT_SRST = MEC17XX_QMSPI_M_SOFT_RESET;
		dummy = MEC17XX_QMSPI0_MODE_ACT_SRST;
		MEC17XX_QMSPI0_MODE_ACT_SRST = 0;
		MEC17XX_PCR_SLP_EN_DEV(MEC17XX_PCR_QMSPI);
	}

	return EC_SUCCESS;
}

