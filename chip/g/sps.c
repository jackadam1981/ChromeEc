/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "pmu.h"
#include "registers.h"
#include "spi.h"
#include "task.h"
#include "timer.h"
#include "watchdog.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_SPI, outstr)
#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SPI, format, ## args)

/*
 * This file is a driver for the CR50 SPS (SPI slave) controller. The
 * controller deploys a 2KB buffer split evenly between receive and transmit
 * directions.
 *
 * Each one kilobyte of memory is organized into a FIFO with read
 * and write pointers. RX FIFO write and TX FIFO read pointers are managed by
 * hardware. RX FIFO read and TX FIFO write pointers are managed by
 * software.
 *
 * As of time of writing, TX fifo allows only 32 bit wide write accesses,
 * which makes the function feeding the FIFO unnecessarily complicated.
 *
 * Even though both FIFOs are 1KByte in size, the hardware pointers
 * controlling access to the FIFOs are 11 bits in size, this is another issue
 * requiring special software handling.
 *
 * The driver API includes three functions:
 *
 * - transmit a packet of a certain size, runs on the task context and can
 *   exit before the entire packet is transmitted.,
 *
 * - register a receive callback. The callback is running in interrupt
 *   context. Registering the callback (re)initializes the interface.
 *
 * - unregister receive callback.
 */

/* SPS Control Mode */
enum sps_mode {
	SPS_GENERIC_MODE = 0,
	SPS_SWETLAND_MODE = 1,
	SPS_ROM_MODE = 2,
	SPS_UNDEF_MODE = 3,
};

#define SPS_FIFO_SIZE		(1 << 10)
#define SPS_FIFO_MASK		(SPS_FIFO_SIZE - 1)
/*
 * Hardware pointers use one extra bit, which means that indexing FIFO and
 * values written into the pointers have to have dfferent sizes. Tracked under
 * http://b/20894690
 */
#define SPS_FIFO_PTR_MASK	((SPS_FIFO_MASK << 1) | 1)

#define SPS_TX_FIFO_BASE_ADDR (GBASE(SPS) + 0x1000)
#define SPS_RX_FIFO_BASE_ADDR (SPS_TX_FIFO_BASE_ADDR + SPS_FIFO_SIZE)

/* SPS Statistic Counters */
static uint32_t sps_tx_count, sps_rx_count, tx_empty_count, max_rx_batch;

/*
 * Push data to the SPS TX FIFO
 * @param inst Interface number
 * @param data Pointer to 8-bit data
 * @param data_size Number of bytes to transmit
 * @return : actual number of bytes placed into tx fifo
 */
int sps_transmit(uint32_t inst, uint8_t *data, size_t data_size)
{
	volatile uint32_t *sps_tx_fifo;
	uint32_t rptr;
	uint32_t wptr;
	uint32_t fifo_room;
	int bytes_sent;

	if (GREAD_FIELD_I(SPS, inst, ISTATE, TXFIFO_EMPTY))
		tx_empty_count++; /* Inside packet this means uderrun. */

	sps_tx_fifo = (volatile uint32_t *)SPS_TX_FIFO_BASE_ADDR;

	wptr = GREG32_I(SPS, inst, TXFIFO_WPTR);
	rptr = GREG32_I(SPS, inst, TXFIFO_RPTR);
	fifo_room = (rptr - wptr - 1) & SPS_FIFO_MASK;

	if (fifo_room < data_size) {
		bytes_sent = fifo_room;
		data_size = fifo_room;
	} else {
		bytes_sent = data_size;
	}

	sps_tx_fifo += (wptr & SPS_FIFO_MASK) / sizeof(*sps_tx_fifo);

	while (data_size) {

		if ((wptr & 3) || (data_size < 4) || ((uintptr_t)data & 3)) {
			/*
			 * Either we have less then 4 bytes to send, or one of
			 * the pointers is not 4 byte aligned. Need to go byte
			 * by byte.
			 */
			uint32_t fifo_contents;
			int bit_shift;

			fifo_contents = *sps_tx_fifo;
			do {
				/*
				 * CR50 SPS controller does not allow byte
				 * accesses for writes into the FIFO, so read
				 * modify/write is requred. Tracked uder
				 * http://b/20894727
				 */
				bit_shift = 8 * (wptr & 3);
				fifo_contents &= ~(0xff << bit_shift);
				fifo_contents |=
					(((uint32_t)(*data++)) << bit_shift);
				data_size--;
				wptr++;

			} while (data_size && (wptr & 3));

			*sps_tx_fifo++ = fifo_contents;
		} else {
			/*
			 * Both fifo wptr and data are aligned and there is
			 * plenty to send.
			 */
			*sps_tx_fifo++ = *((uint32_t *)data);
			data += 4;
			data_size -= 4;
			wptr += 4;
		}
		GREG32_I(SPS, inst, TXFIFO_WPTR) = wptr & SPS_FIFO_PTR_MASK;

		/* Make sure FIFO pointer wraps along with the index. */
		if (!(wptr & SPS_FIFO_MASK))
			sps_tx_fifo = (volatile uint32_t *)
				SPS_TX_FIFO_BASE_ADDR;
	}

	/*
	 * Start TX if necessary. This happens after FIFO is primed, which
	 * helps aleviate TX underrun problems but introduces delay before
	 * data starts coming out.
	 */
	if (!GREAD_FIELD(SPS, FIFO_CTRL, TXFIFO_EN))
		GWRITE_FIELD(SPS, FIFO_CTRL, TXFIFO_EN, 1);

	sps_tx_count += bytes_sent;
	return bytes_sent;
}

static void sps_reset(void)
{
	enum sps_mode mode = SPS_GENERIC_MODE;
	enum spi_clock_mode clk_mode = SPI_CLOCK_MODE0;

	/* Disable All Interrupts */
	GREG32(SPS, ICTRL) = 0;

	GWRITE_FIELD(SPS, CTRL, MODE, mode);
	GWRITE_FIELD(SPS, CTRL, IDLE_LVL, 0);
	GWRITE_FIELD(SPS, CTRL, CPHA, clk_mode & 1);
	GWRITE_FIELD(SPS, CTRL, CPOL, (clk_mode >> 1) & 1);
	GWRITE_FIELD(SPS, CTRL, TXBITOR, 1); /* MSB first */
	GWRITE_FIELD(SPS, CTRL, RXBITOR, 1); /* MSB first */
	/* xfer 0xff when tx fifo is empty */
	GREG32(SPS, DUMMY_WORD) = 0xff;

	/* [5,4,3]           [2,1,0]
	 * RX{DIS, EN, RST} TX{DIS, EN, RST}
	 */
	GREG32(SPS, FIFO_CTRL) = 0x9;

	/* wait for reset to self clear. */
	while (GREG32(SPS, FIFO_CTRL) & 9)
		;
}

static void sps_rx_enable(void)
{
	/* We don't enable TX FIFO until we have something to send. */
	GWRITE_FIELD(SPS, FIFO_CTRL, RXFIFO_EN, 1);

	/*
	 * Wait until we have a few bytes in the FIFO before waking up. Note
	 * that if the host wants to read bytes from us, it may have to clock
	 * in at least RXFIFO_THRESHOLD+1 bytes before we notice that it's
	 * asking.
	 */
	GREG32(SPS, RXFIFO_THRESHOLD) = 8;
	GWRITE_FIELD(SPS, ICTRL, RXFIFO_LVL, 1);

	/* Also wake up when the host has finished talking to us, so we can
	 * drain any remaining bytes in the RX FIFO. Too late for TX, of
	 * course. */
	GWRITE_FIELD(SPS, ISTATE_CLR, CS_DEASSERT, 1);
	GWRITE_FIELD(SPS, ICTRL, CS_DEASSERT, 1);
}


/*
 * RX interrupt callback function prototype. This function returns a portion
 * of the received SPI data and current status of the CS line. When CS is
 * deasserted, this function is called with data_size of zero and a non-zero
 * cs_status. This allows the recipient to delineate the SPS frames.
 */
typedef void (*rx_handler_f)(uint32_t inst, uint8_t *data,
			     size_t data_size, int cs_status);

/*
 * Register and unregister rx_handler. Side effects of registering the handler
 * is reinitializing the interface.
 */
static rx_handler_f sps_rx_handler;

int sps_register_rx_handler(rx_handler_f rx_handler)
{
	if (sps_rx_handler)
		return -1;

	sps_rx_handler = rx_handler;
	sps_reset();
	sps_rx_enable();
	task_enable_irq(GC_IRQNUM_SPS0_RXFIFO_LVL_INTR);
	task_enable_irq(GC_IRQNUM_SPS0_CS_DEASSERT_INTR);

	CPRINTS("Reset SPS module");

	return 0;
}

int sps_unregister_rx_handler(void)
{
	if (!sps_rx_handler)
		return -1;

	task_disable_irq(GC_IRQNUM_SPS0_RXFIFO_LVL_INTR);
	task_disable_irq(GC_IRQNUM_SPS0_CS_DEASSERT_INTR);
	sps_reset();
	sps_rx_handler = NULL;
	return 0;
}

/*****************************************************************************/
/* Interrupt handler stuff */

/*
 * Check how much data is available in RX FIFO and return pointer to the
 * available data and its size.
 *
 * @param inst Interface number
 * @param data - pointer to set to the beginning of data in the fifo
 * @return number of available bytes and the sets the pointer if number of
 *         bytes is non zero
 */
static int sps_check_rx(uint32_t inst, uint8_t **data)
{
	uint32_t write_ptr = GREG32_I(SPS, inst, RXFIFO_WPTR) & SPS_FIFO_MASK;
	uint32_t read_ptr = GREG32_I(SPS, inst, RXFIFO_RPTR) & SPS_FIFO_MASK;

	if (read_ptr == write_ptr)
		return 0;

	*data = (uint8_t *)(SPS_RX_FIFO_BASE_ADDR + read_ptr);

	if (read_ptr > write_ptr)
		return SPS_FIFO_SIZE - read_ptr;

	return write_ptr - read_ptr;
}

/* Advance RX FIFO read pointer after data has been read from the FIFO. */
static void sps_advance_rx(int port, int data_size)
{
	uint32_t read_ptr = GREG32_I(SPS, port, RXFIFO_RPTR) + data_size;

	GREG32_I(SPS, port, RXFIFO_RPTR) = read_ptr & SPS_FIFO_PTR_MASK;
}

/*
 * Actual receive interrupt processing function. Invokes the callback passing
 * it a pointer to the linear space in the RX FIFO and the number of bytes
 * availabe at that address.
 *
 * If RX fifo is wrapping around, the callback will be called twice with two
 * flat pointers.
 *
 * If the CS has been deasseted, after all remaining RX FIFO data has been
 * passed to the callback, the callback is called one last time with zero data
 * size and the CS indication, this allows the client to delineate received
 * packets.
 */
static void sps_rx_interrupt(uint32_t port, int cs_deasserted)
{
	for (;;) {
		uint8_t *received_data;
		size_t data_size;

		data_size = sps_check_rx(port, &received_data);
		if (!data_size)
			break;

		sps_rx_count += data_size;

		if (sps_rx_handler)
			sps_rx_handler(port, received_data, data_size, 0);

		if (data_size > max_rx_batch)
			max_rx_batch = data_size;

		sps_advance_rx(port, data_size);
	}

	if (cs_deasserted)
		sps_rx_handler(port, NULL, 0, 1);
}

void _sps0_interrupt(void)
{
	sps_rx_interrupt(0, 0);
	/* The RXFIFO_LVL interrupt clears itself when the level drops */
}
DECLARE_IRQ(GC_IRQNUM_SPS0_RXFIFO_LVL_INTR, _sps0_interrupt, 1);

void _sps0_cs_deassert_interrupt(void)
{
	/* Make sure the receive FIFO is drained. */
	sps_rx_interrupt(0, 1);
	/* Clear the interrupt bit */
	GWRITE_FIELD(SPS, ISTATE_CLR, CS_DEASSERT, 1);
	/* Disable transmission, in case the host lost interest early */
	GWRITE_FIELD(SPS, FIFO_CTRL, TXFIFO_EN, 0);
}
DECLARE_IRQ(GC_IRQNUM_SPS0_CS_DEASSERT_INTR, _sps0_cs_deassert_interrupt, 1);

/* RX FIFO handler (runs in interrupt context) */
static void sps_receive_callback(uint32_t inst, uint8_t *data,
				 size_t data_size, int cs_status)
{
	static uint8_t buf[1024];		/* probably not necessary */
	uint8_t *bufptr = buf;

	if (!data_size)
		return;

	/* When bytes show up, just echo them right back out again */
	memcpy(bufptr, data, data_size);
	while (data_size) {
		size_t cnt = sps_transmit(inst, bufptr, data_size);
		data_size -= cnt;
		bufptr += cnt;
	}
}

static void sps_init(void)
{
	pmu_clock_en(PERIPH_SPS);
	sps_register_rx_handler(sps_receive_callback);
}
DECLARE_HOOK(HOOK_INIT, sps_init, HOOK_PRIO_DEFAULT);


static int command_sps(int argc, char **argv)
{
	int i;

	if (argc < 2) {
		sps_unregister_rx_handler();
		sps_register_rx_handler(sps_receive_callback);
		return EC_SUCCESS;
	}

	for (i = 1; i < argc; i++)
		sps_transmit(0, argv[i], strlen(argv[i]));

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(sps, command_sps,
			"[STRING]",
			"With no args, reset the FIFOs. "
			"Otherwise, transmit the string.",
			NULL);
