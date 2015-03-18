/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "clock.h"
#include "common.h"
#include "console.h"
#include "dma.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "i2c_arbitration.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_I2C, outstr)
#define CPRINTF(format, args...) cprintf(CC_I2C, format, ## args)
#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)

/* I2C bus frequency */
#define I2C_FREQ 100000 /* Hz */

/* I2C bit period in microseconds */
#define I2C_PERIOD_US (SECOND / I2C_FREQ)

/*
 * Transmit timeout in microseconds
 *
 * In theory we shouldn't have a timeout here (at least when we're in slave
 * mode).  The slave is supposed to wait forever for the master to read bytes.
 * ...but we're going to keep the timeout to make sure we're robust.  It may in
 * fact be needed if the host resets itself mid-read.
 *
 * NOTE: One case where this timeout is useful is when the battery
 * flips out.  The battery may flip out and hold lines low for up to
 * 25ms.  If we just wait it will eventually let them go.
 */
#define I2C_TX_TIMEOUT_SLAVE	(100 * MSEC)
#define I2C_TX_TIMEOUT_MASTER	(30 * MSEC)

/*
 * We delay 5us in bitbang mode.  That gives us 5us low and 5us high or
 * a frequency of 100kHz.
 *
 * Note that the code takes a little time to run so we don't actually get
 * 100kHz, but that's OK.
 */
#define I2C_BITBANG_DELAY_US	5

/* Select the DMA channels matching the board configuration */
#define DMAC_SLAVE_TX \
	((I2C_PORT_SLAVE == STM32_I2C2_PORT) ? \
	 STM32_DMAC_I2C2_TX : STM32_DMAC_I2C1_TX)
#define DMAC_SLAVE_RX \
	((I2C_PORT_SLAVE == STM32_I2C2_PORT) ? \
	 STM32_DMAC_I2C2_RX : STM32_DMAC_I2C1_RX)
#define DMAC_MASTER_TX \
	((I2C_PORT_MASTER == STM32_I2C2_PORT) ? \
	 STM32_DMAC_I2C2_TX : STM32_DMAC_I2C1_TX)
#define DMAC_MASTER_RX \
	((I2C_PORT_MASTER == STM32_I2C2_PORT) ? \
	 STM32_DMAC_I2C2_RX : STM32_DMAC_I2C1_RX)

#ifdef CONFIG_HOSTCMD_I2C_SLAVE_ADDR
#if (I2C_PORT_SLAVE == STM32_I2C2_PORT)
#define IRQ_SLAVE_EV STM32_IRQ_I2C2_EV
#define IRQ_SLAVE_ER STM32_IRQ_I2C2_ER
#elif (I2C_PORT_SLAVE == STM32_I2C1_PORT)
#define IRQ_SLAVE_EV STM32_IRQ_I2C1_EV
#define IRQ_SLAVE_ER STM32_IRQ_I2C1_ER
#else
#error "Not implemented"
#endif
#endif

enum {
	/*
	 * A stop condition should take 2 clocks, but the process may need more
	 * time to notice if it is preempted, so we poll repeatedly for 8
	 * clocks, before backing off and only check once every
	 * STOP_SENT_RETRY_US for up to TIMEOUT_STOP_SENT clocks before giving
	 * up.
	 */
	SLOW_STOP_SENT_US	= I2C_PERIOD_US * 8,
	TIMEOUT_STOP_SENT_US	= I2C_PERIOD_US * 200,
	STOP_SENT_RETRY_US	= 150,
};

static const struct dma_option dma_tx_option[I2C_PORT_COUNT] = {
	{STM32_DMAC_I2C1_TX, (void *)&STM32_I2C_DR(STM32_I2C1_PORT),
	 STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT},
	{STM32_DMAC_I2C2_TX, (void *)&STM32_I2C_DR(STM32_I2C2_PORT),
	 STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT},
};

static const struct dma_option dma_rx_option[I2C_PORT_COUNT] = {
	{STM32_DMAC_I2C1_RX, (void *)&STM32_I2C_DR(STM32_I2C1_PORT),
	 STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT},
	{STM32_DMAC_I2C2_RX, (void *)&STM32_I2C_DR(STM32_I2C2_PORT),
	 STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT},
};

static inline void disable_i2c_interrupt(int port)
{
	STM32_I2C_CR2(port) &= ~(STM32_I2C_CR2_ITERREN | STM32_I2C_CR2_ITEVTEN);
}

static inline void enable_i2c_interrupt(int port)
{
	STM32_I2C_CR2(port) |= STM32_I2C_CR2_ITERREN | STM32_I2C_CR2_ITEVTEN;
}

static inline void enable_ack(int port)
{
	STM32_I2C_CR1(port) |= STM32_I2C_CR1_ACK;
}

static inline void disable_ack(int port)
{
	STM32_I2C_CR1(port) &= ~STM32_I2C_CR1_ACK;
}

static inline void dump_i2c_reg(int port)
{
#ifdef CONFIG_I2C_DEBUG
	CPRINTF("CR1(%d)  : %016b\n", port, STM32_I2C_CR1(port));
	CPRINTF("CR2(%d)  : %016b\n", port, STM32_I2C_CR2(port));
	CPRINTF("SR2(%d)  : %016b\n", port, STM32_I2C_SR2(port));
	CPRINTF("SR1(%d)  : %016b\n", port, STM32_I2C_SR1(port));
	CPRINTF("OAR1(%d) : %016b\n", port, STM32_I2C_OAR1(port));
	CPRINTF("OAR2(%d) : %016b\n", port, STM32_I2C_OAR2(port));
	CPRINTF("DR(%d)   : %016b\n", port, STM32_I2C_DR(port));
	CPRINTF("CCR(%d)  : %016b\n", port, STM32_I2C_CCR(port));
	CPRINTF("TRISE(%d): %016b\n", port, STM32_I2C_TRISE(port));
#endif /* CONFIG_I2C_DEBUG */
}

static void i2c_init_port(unsigned int port);

#ifdef CONFIG_HOSTCMD_I2C_SLAVE_ADDR
static uint16_t i2c_sr1[I2C_PORT_COUNT];
/* Flag indicating if a command is currently in the buffer */
static uint8_t rx_pending;

/* Buffer for host commands (including version, error code and checksum) */
static uint8_t host_buffer[I2C_MAX_HOST_PACKET_SIZE + 2];
static uint8_t params_copy[I2C_MAX_HOST_PACKET_SIZE] __aligned(4);
static struct host_packet i2c_packet;


static int i2c_write_raw_slave(int port, void *buf, int len)
{
	dma_chan_t *chan;
	int rv;

	/* we don't want to race with TxE interrupt event */
	disable_i2c_interrupt(port);

	/* Configuring DMA1 channel DMAC_SLAVE_TX */
	enable_ack(port);
	chan = dma_get_channel(DMAC_SLAVE_TX);
	dma_prepare_tx(dma_tx_option + port, len, buf);

	/* Start the DMA */
	dma_go(chan);

	/* Configuring i2c to use DMA */
	STM32_I2C_CR2(port) |= STM32_I2C_CR2_DMAEN;

	if (in_interrupt_context()) {
		/* Poll for the transmission complete flag */
		dma_wait(DMAC_SLAVE_TX);
		dma_clear_isr(DMAC_SLAVE_TX);
	} else {
		/* Wait for the transmission complete Interrupt */
		dma_enable_tc_interrupt(DMAC_SLAVE_TX);
		rv = task_wait_event_mask(
				TASK_EVENT_DMA_TC, DMA_TRANSFER_TIMEOUT_US);
		dma_disable_tc_interrupt(DMAC_SLAVE_TX);

		if (!(rv & TASK_EVENT_DMA_TC)) {
			CPRINTS("Slave timeout, resetting i2c");
			i2c_init_port(port);
		}
	}

	dma_disable(DMAC_SLAVE_TX);
	STM32_I2C_CR2(port) &= ~STM32_I2C_CR2_DMAEN;

	enable_i2c_interrupt(port);

	return len;
}

/* Process the command in the i2c host buffer */
static void i2c_send_response_packet(struct host_packet *pkt)
{
	int size = pkt->response_size;
	uint8_t *out = host_buffer;

	/* Ignore host command in-progress */
	if (pkt->driver_result == EC_RES_IN_PROGRESS)
		return;

	/* Write result and size to first two bytes. */
	*out++ = pkt->driver_result;
	*out++ = size;

	/* send the answer to the AP */
	i2c_write_raw_slave(I2C_PORT_SLAVE, host_buffer, size + 2);
}

static void i2c_process_command(void)
{
	char *buff = host_buffer;

	/*
	 * TODO(crosbug.com/p/29241): Combine this functionality with the
	 * i2c_process_command function in chip/stm32/i2c-stm32f.c to make one
	 * host command i2c process function which handles all protocol
	 * versions.
	 */
	i2c_packet.send_response = i2c_send_response_packet;

	i2c_packet.request = (const void *)(&buff[1]);
	i2c_packet.request_temp = params_copy;
	i2c_packet.request_max = sizeof(params_copy);
	/* Don't know the request size so pass in the entire buffer */
	i2c_packet.request_size = I2C_MAX_HOST_PACKET_SIZE;

	/*
	 * Stuff response at buff[2] to leave the first two bytes of
	 * buffer available for the result and size to send over i2c.
	 */
	i2c_packet.response = (void *)(&buff[2]);
	i2c_packet.response_max = I2C_MAX_HOST_PACKET_SIZE;
	i2c_packet.response_size = 0;

	if (*buff >= EC_COMMAND_PROTOCOL_3) {
		i2c_packet.driver_result = EC_RES_SUCCESS;
	} else {
		/* Only host command protocol 3 is supported. */
		i2c_packet.driver_result = EC_RES_INVALID_HEADER;
	}
	host_packet_receive(&i2c_packet);
}

static void i2c_event_handler(int port)
{
	/* save and clear status */
	i2c_sr1[port] = STM32_I2C_SR1(port);
	STM32_I2C_SR1(port) = 0;

	/* Confirm that you are not in master mode */
	if (STM32_I2C_SR2(port) & STM32_I2C_SR2_MSL) {
		CPRINTS("slave ISR triggered in master mode, ignoring");
		return;
	}

	/* transfer matched our slave address */
	if (i2c_sr1[port] & STM32_I2C_SR1_ADDR) {
		/* If it's a receiver slave */
		if (!(STM32_I2C_SR2(port) & STM32_I2C_SR2_TRA)) {
			dma_start_rx(dma_rx_option + port, sizeof(host_buffer),
				     host_buffer);

			STM32_I2C_CR2(port) |= STM32_I2C_CR2_DMAEN;
			rx_pending = 1;
		}

		/* cleared by reading SR1 followed by reading SR2 */
		STM32_I2C_SR1(port);
		STM32_I2C_SR2(port);
	} else if (i2c_sr1[port] & STM32_I2C_SR1_STOPF) {
		/* If it's a receiver slave */
		if (!(STM32_I2C_SR2(port) & STM32_I2C_SR2_TRA)) {
			/* Disable, and clear the DMA transfer complete flag */
			dma_disable(DMAC_SLAVE_RX);
			dma_clear_isr(DMAC_SLAVE_RX);

			/* Turn off i2c's DMA flag */
			STM32_I2C_CR2(port) &= ~STM32_I2C_CR2_DMAEN;
		}
		/* clear STOPF bit by reading SR1 and then writing CR1 */
		STM32_I2C_SR1(port);
		STM32_I2C_CR1(port) = STM32_I2C_CR1(port);
	}

	/* TxE event */
	if (i2c_sr1[port] & STM32_I2C_SR1_TXE) {
		if (port == I2C_PORT_SLAVE) { /* AP waits for EC response */
			if (rx_pending) {
				i2c_process_command();
				/* reset host buffer after end of transfer */
				rx_pending = 0;
			} else {
				/* spurious read : return dummy value */
				STM32_I2C_DR(port) = 0xec;
			}
		}
	}
}
void i2c_slave_event_interrupt(void) { i2c_event_handler(I2C_PORT_SLAVE); }
DECLARE_IRQ(IRQ_SLAVE_EV, i2c_slave_event_interrupt, 3);

static void i2c_error_handler(int port)
{
	i2c_sr1[port] = STM32_I2C_SR1(port);

	if (i2c_sr1[port] & STM32_I2C_SR1_AF) {
		/* ACK failed (NACK); expected when AP reads final byte.
		 * Software must clear AF bit. */
	} else {
		CPRINTS("%s: I2C_SR1(%d): 0x%04x",
			__func__, port, i2c_sr1[port]);
		CPRINTS("%s: I2C_SR2(%d): 0x%04x",
			__func__, port, STM32_I2C_SR2(port));
	}

	STM32_I2C_SR1(port) &= ~0xdf00;
}
void i2c_slave_error_interrupt(void) { i2c_error_handler(I2C_PORT_SLAVE); }
DECLARE_IRQ(IRQ_SLAVE_ER, i2c_slave_error_interrupt, 2);
#endif

/* board-specific setup for post-I2C module init */
void __board_i2c_post_init(int port)
{
}

void board_i2c_post_init(int port)
		__attribute__((weak, alias("__board_i2c_post_init")));

static void i2c_init_port(unsigned int port)
{
	const int i2c_clock_bit[] = { STM32_RCC_PB1_I2C1, STM32_RCC_PB1_I2C2 };
	int freq = clock_get_freq(STM32_I2C_PERIPH_CLASS(port));

	if (!(STM32_RCC_APB1ENR & i2c_clock_bit[port])) {
		/* Only unwedge the bus if the clock is off */
		if (i2c_claim(port) == EC_SUCCESS) {
			i2c_release(port);
		}

		/* enable I2C2 clock */
		STM32_RCC_APB1ENR |= i2c_clock_bit[port];
		/* Delay 1 APB clock cycle after the clock is enabled */
		clock_wait_bus_cycles(BUS_APB, 1);
	}

	/* force reset of the i2c peripheral */
	STM32_I2C_CR1(port) |= STM32_I2C_CR1_SWRST;
	STM32_I2C_CR1(port) &= ~STM32_I2C_CR1_SWRST;

	/* set clock configuration : standard mode (100kHz) */
	STM32_I2C_CR2(port) = freq / SECOND;
	STM32_I2C_CCR(port) = freq / (2 * I2C_FREQ);
	STM32_I2C_TRISE(port) = freq / SECOND + 1;

#ifdef CONFIG_HOSTCMD_I2C_SLAVE_ADDR
	/* set slave address */
	if (port == I2C_PORT_SLAVE)
		STM32_I2C_OAR1(port) = CONFIG_HOSTCMD_I2C_SLAVE_ADDR;
#endif

	/* configuration : I2C mode / Periphal enabled, ACK enabled */
	STM32_I2C_CR1(port) |= STM32_I2C_CR1_ACK | STM32_I2C_CR1_PE;

	/* clear status */
	STM32_I2C_SR1(port) = 0;

	board_i2c_post_init(port);
	enable_i2c_interrupt(port);
}

static void i2c_init(void)
{
	const struct i2c_port_t *p = i2c_ports;
	int i;

	/*
	 * TODO(crosbug.com/p/23763): Add config options to determine which
	 * channels to init.
	 */
	for (i = 0; i < i2c_ports_used; i++, p++) {
		i2c_init_port(p->port);
#ifdef CONFIG_HOSTCMD_I2C_SLAVE_ADDR
		if (p->port == I2C_PORT_SLAVE) {
			/* Enable event and error interrupts */
			task_enable_irq(IRQ_SLAVE_EV);
			task_enable_irq(IRQ_SLAVE_ER);
		}
#endif
	}

	/* Configure GPIOs */
	gpio_config_module(MODULE_I2C, 1);
}
DECLARE_HOOK(HOOK_INIT, i2c_init, HOOK_PRIO_INIT_I2C);

/*****************************************************************************/
/* STM32 Host I2C */
enum wait_t {
	WAIT_NONE,
	WAIT_MASTER_START,
	WAIT_ADDR_READY,
	WAIT_XMIT_TXE,
	WAIT_XMIT_FINAL_TXE,
	WAIT_XMIT_BTF,
	WAIT_XMIT_STOP,
	WAIT_RX_NE,
	WAIT_RX_NE_FINAL,
	WAIT_RX_NE_STOP,
	WAIT_RX_NE_STOP_SIZE2,
	WAIT_DMA_DONE,
};

/**
 * Wait for a specific i2c event
 *
 * This function waits until the bit(s) corresponding to mask in
 * the specified port's I2C SR1 register is/are set.  It may
 * return a timeout or success.
 *
 * @param port Port to wait on
 * @param mask A mask specifying which bits in SR1 to wait to be set
 * @param wait A wait code to be returned with the timeout error code if that
 *             occurs, to help with debugging.
 * @return EC_SUCCESS, or EC_ERROR_TIMEOUT with the wait code OR'd onto the
 *             bits 8-16 to indicate what it timed out waiting for.
 */
static int wait_status(int port, uint32_t mask, enum wait_t wait)
{
	uint32_t r;
	timestamp_t t1, t2;

	t1 = t2 = get_time();
	r = STM32_I2C_SR1(port);
	while (mask ? ((r & mask) != mask) : r) {
		t2 = get_time();

		if (t2.val - t1.val > I2C_TX_TIMEOUT_MASTER)
			return EC_ERROR_TIMEOUT | (wait << 8);
		else if (t2.val - t1.val > 150)
			usleep(100);

		r = STM32_I2C_SR1(port);
	}

	return EC_SUCCESS;
}

static inline uint32_t read_clear_status(int port)
{
	uint32_t sr1, sr2;

	sr1 = STM32_I2C_SR1(port);
	sr2 = STM32_I2C_SR2(port);
	return (sr2 << 16) | (sr1 & 0xffff);
}

static int master_start(int port, int slave_addr)
{
	int rv;

	/* Change to master send mode, reset stop bit, send start bit */
	STM32_I2C_CR1(port) = (STM32_I2C_CR1(port) & ~STM32_I2C_CR1_STOP) |
		STM32_I2C_CR1_START;
	/* Wait for start bit sent event */
	rv = wait_status(port, STM32_I2C_SR1_SB, WAIT_MASTER_START);
	if (rv)
		return rv;

	/* Send address */
	STM32_I2C_DR(port) = slave_addr;
	/* Wait for addr ready */
	rv = wait_status(port, STM32_I2C_SR1_ADDR, WAIT_ADDR_READY);
	if (rv)
		return rv;

	read_clear_status(port);

	return EC_SUCCESS;
}

static void master_stop(int port)
{
	STM32_I2C_CR1(port) |= STM32_I2C_CR1_STOP;
}

static int wait_until_stop_sent(int port)
{
	timestamp_t deadline;
	timestamp_t slow_cutoff;
	uint8_t is_slow;

	deadline = slow_cutoff = get_time();
	deadline.val += TIMEOUT_STOP_SENT_US;
	slow_cutoff.val += SLOW_STOP_SENT_US;

	while (STM32_I2C_CR1(port) & STM32_I2C_CR1_STOP) {
		if (timestamp_expired(deadline, NULL)) {
			ccprintf("Stop event deadline passed:\ttask=%d"
							"\tCR1=%016b\n",
				(int)task_get_current(), STM32_I2C_CR1(port));
			return EC_ERROR_TIMEOUT;
		}

		if (is_slow) {
			/* If we haven't gotten a fast response, sleep */
			usleep(STOP_SENT_RETRY_US);
		} else {
			/* Check to see if this request is taking a while */
			if (timestamp_expired(slow_cutoff, NULL)) {
				ccprintf("Stop event taking a while: task=%d",
					(int)task_get_current());
				is_slow = 1;
			}
		}
	}

	return EC_SUCCESS;
}

static int handle_i2c_error(int port, int rv)
{
	timestamp_t t1, t2;
	uint32_t r;

	/* We have not used the bus, just exit */
	if (rv == EC_ERROR_BUSY)
		return rv;

	/* EC_ERROR_TIMEOUT may have a code specifying where the timeout was */
	if ((rv & 0xff) == EC_ERROR_TIMEOUT) {
#ifdef CONFIG_I2C_DEBUG
		CPRINTS("Wait_status() timeout type: %d", (rv >> 8));
#endif
		if ((rv >> 8) == WAIT_ADDR_READY)
			rv = EC_ERROR_NOT_PRESENT;
		else
			rv = EC_ERROR_TIMEOUT;
	}
	if (rv)
		dump_i2c_reg(port);

	/* Clear rc_w0 bits */
	STM32_I2C_SR1(port) = 0;
	/* Clear seq read status bits */
	r = STM32_I2C_SR1(port);
	r = STM32_I2C_SR2(port);
	/* Clear busy state */
	t1 = get_time();

	if ((rv == EC_ERROR_TIMEOUT) &&
	    (STM32_I2C_CR1(port) & STM32_I2C_CR1_START)) {
		/*
		 * If it failed while just trying to send the start bit then
		 * something is wrong with the internal state of the i2c,
		 * (Probably a stray pulse on the line got it out of sync with
		 * the actual bytes) so reset it.
		 */
		CPRINTS("Unable to send START, resetting i2c");
		i2c_init_port(port);
		goto cr_cleanup;
	} else if (rv == EC_ERROR_TIMEOUT && !(r & 2)) {
		/*
		 * If the BUSY bit is faulty, send a stop bit just to be sure.
		 * It seems that this can be happen very briefly while sending
		 * a 1. We've not actually seen this, but just to be safe.
		 */
		CPRINTS("Bad BUSY bit detected");
		master_stop(port);
	}

	/* Try to send stop bits until the bus becomes idle */
	while (r & 2) {
		t2 = get_time();
		if (t2.val - t1.val > I2C_TX_TIMEOUT_MASTER) {
			dump_i2c_reg(port);
			/* Reset the i2c periph to get it back to slave mode */
			i2c_init_port(port);
			goto cr_cleanup;
		}
		/* Send stop */
		master_stop(port);
		usleep(1000);
		r = STM32_I2C_SR2(port);
	}

cr_cleanup:
	/*
	 * Reset control register to the default state :
	 * I2C mode / Periphal enabled, ACK enabled
	 */
	STM32_I2C_CR1(port) = STM32_I2C_CR1_ACK | STM32_I2C_CR1_PE;
	return rv;
}

static int i2c_master_transmit(int port, int slave_addr, const uint8_t *data,
			       int size, int stop)
{
	int rv = 0, rv_start;

	disable_ack(port);
	/* Configure DMA channel for TX to host */
	dma_prepare_tx(dma_tx_option + port, size, data);
	dma_enable_tc_interrupt(DMAC_MASTER_TX);

	/* Start the DMA */
	dma_go(dma_get_channel(DMAC_MASTER_TX));

	/* Configuring i2c2 to use DMA */
	STM32_I2C_CR2(port) |= STM32_I2C_CR2_DMAEN;

	/* Initialise i2c communication by sending START and ADDR */
	rv_start = master_start(port, slave_addr);

	/* If it started, wait for the transmission complete Interrupt */
	if (!rv_start)
		rv = task_wait_event_mask(
				TASK_EVENT_DMA_TC, DMA_TRANSFER_TIMEOUT_US);

	dma_disable(DMAC_MASTER_TX);
	dma_disable_tc_interrupt(DMAC_MASTER_TX);
	STM32_I2C_CR2(port) &= ~STM32_I2C_CR2_DMAEN;
	if (rv_start)
		return rv_start;
	if (!(rv & TASK_EVENT_DMA_TC))
		return EC_ERROR_TIMEOUT | (WAIT_DMA_DONE << 8);

	rv = wait_status(port, STM32_I2C_SR1_BTF, WAIT_XMIT_BTF);
	if (rv)
		return rv;

	if (stop) {
		master_stop(port);
		return wait_status(port, 0, WAIT_XMIT_STOP);
	}

	return EC_SUCCESS;
}

static int i2c_master_receive(int port, int slave_addr, uint8_t *data,
			      int size)
{
	int rv, rv_start;

	if (data == NULL || size < 1)
		return EC_ERROR_INVAL;

	/* Master receive only supports DMA for payloads > 1 byte */
	if (size > 1) {
		enable_ack(port);
		dma_start_rx(dma_rx_option + port, size, data);

		dma_enable_tc_interrupt(DMAC_MASTER_RX);

		STM32_I2C_CR2(port) |= STM32_I2C_CR2_DMAEN;
		STM32_I2C_CR2(port) |= STM32_I2C_CR2_LAST;

		rv_start = master_start(port, slave_addr | 1);
		if (!rv_start)
			rv = task_wait_event_mask(TASK_EVENT_DMA_TC,
					DMA_TRANSFER_TIMEOUT_US);

		dma_disable(DMAC_MASTER_RX);
		dma_disable_tc_interrupt(DMAC_MASTER_RX);
		STM32_I2C_CR2(port) &= ~STM32_I2C_CR2_DMAEN;
		disable_ack(port);

		if (rv_start)
			return rv_start;
		if (!(rv & TASK_EVENT_DMA_TC))
			return EC_ERROR_TIMEOUT | (WAIT_DMA_DONE << 8);

		master_stop(port);
	} else {
		disable_ack(port);

		rv = master_start(port, slave_addr | 1);
		if (rv)
			return rv;
		master_stop(port);
		rv = wait_status(port, STM32_I2C_SR1_RXNE,
				 WAIT_RX_NE_STOP_SIZE2);
		if (rv)
			return rv;
		data[0] = STM32_I2C_DR(port);
	}

	return wait_until_stop_sent(port);
}

int i2c_xfer(int port, int slave_addr, const uint8_t *out, int out_bytes,
	     uint8_t *in, int in_bytes, int flags)
{
	int rv;

	ASSERT(out || !out_bytes);
	ASSERT(in || !in_bytes);

	if (i2c_claim(port))
		return EC_ERROR_BUSY;

	/* If the port appears to be wedged, then try to unwedge it. */
	if (!i2c_raw_get_scl(port) || !i2c_raw_get_sda(port)) {
		i2c_unwedge(port);

		/* Reset the i2c port. */
		i2c_init_port(port);
	}

	disable_i2c_interrupt(port);

	if (out_bytes)
		rv = i2c_master_transmit(port, slave_addr, out, out_bytes,
				in_bytes ? 0 : 1);
	if (!rv && in_bytes)
		rv = i2c_master_receive(port, slave_addr, in, in_bytes);
	rv = handle_i2c_error(port, rv);

	enable_i2c_interrupt(port);

	i2c_release(port);

	return rv;
}

int i2c_raw_get_scl(int port)
{
	enum gpio_signal g;

	if (get_scl_from_i2c_port(port, &g) == EC_SUCCESS)
		return gpio_get_level(g);

	/* If no SCL pin defined for this port, then return 1 to appear idle. */
	return 1;
}

int i2c_raw_get_sda(int port)
{
	enum gpio_signal g;

	if (get_sda_from_i2c_port(port, &g) == EC_SUCCESS)
		return gpio_get_level(g);

	/* If no SDA pin defined for this port, then return 1 to appear idle. */
	return 1;
}

int i2c_get_line_levels(int port)
{
	return (i2c_raw_get_sda(port) ? I2C_LINE_SDA_HIGH : 0) |
		(i2c_raw_get_scl(port) ? I2C_LINE_SCL_HIGH : 0);
}

