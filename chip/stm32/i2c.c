/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "board.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "message.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_I2C, outstr)
#define CPRINTF(format, args...) cprintf(CC_I2C, format, ## args)

/* 8-bit I2C slave address */
#define I2C_ADDRESS 0x3c

/* I2C bus frequency */
#define I2C_FREQ 100000 /* Hz */

/* Clock divider for I2C controller */
#define I2C_CCR (CPU_CLOCK/(2 * I2C_FREQ))

#define NUM_PORTS 2
#define I2C1      STM32_I2C1_PORT
#define I2C2      STM32_I2C2_PORT

static task_id_t task_waiting_on_port[NUM_PORTS];
static struct mutex port_mutex[NUM_PORTS];

static uint16_t i2c_sr1[NUM_PORTS];

/* per-transaction counters */
static unsigned int tx_byte_count;
static unsigned int rx_byte_count;

/*
 * i2c_xmit_mode determines what EC sends when AP initiates a
 * read transaction. If AP has not set a transmit mode, then
 * default to NOP.
 */
static enum message_cmd_t i2c_xmit_mode[NUM_PORTS] = { CMDC_NOP, CMDC_NOP };

/*
 * Our output buffers. These must be large enough for our largest message,
 * including protocol overhead.
 */
static uint8_t out_msg[32];


static void wait_rx(int port)
{
	/* TODO: Add timeouts and error checking for safety */
	while (!(STM32_I2C_SR1(port) & (1 << 6)))
		;
}

static void wait_tx(int port)
{
	/* TODO: Add timeouts and error checking for safety */
	while (!(STM32_I2C_SR1(port) & (1 << 7)))
		;
}

static int i2c_read_raw(int port, void *buf, int len)
{
	int i;
	uint8_t *data = buf;

	mutex_lock(&port_mutex[port]);
	rx_byte_count = 0;
	for (i = 0; i < len; i++) {
		wait_rx(port);
		data[i] = STM32_I2C_DR(port);
		rx_byte_count++;
	}
	mutex_unlock(&port_mutex[port]);

	return len;
}

static int i2c_write_raw(int port, void *buf, int len)
{
	int i;
	uint8_t *data = buf;

	mutex_lock(&port_mutex[port]);
	tx_byte_count = 0;
	for (i = 0; i < len; i++) {
		tx_byte_count++;
		STM32_I2C_DR(port) = data[i];
		wait_tx(port);
	}
	mutex_unlock(&port_mutex[port]);

	return len;
}

void i2c2_work_task(void)
{
	int msg_len;
	uint16_t tmp16;
	task_waiting_on_port[1] = task_get_current();

	while (1) {
		task_wait_event(-1);
		tmp16 = i2c_sr1[I2C2];
		if (tmp16 & (1 << 6)) {
			/* RxNE; AP issued write command */
			i2c_read_raw(I2C2, &i2c_xmit_mode[I2C2], 1);
#ifdef CONFIG_DEBUG
			CPRINTF("%s: i2c2_xmit_mode: %02x\n",
					__func__, i2c_xmit_mode[I2C2]);
#endif
		} else if (tmp16 & (1 << 7)) {
			/* RxE; AP is waiting for EC response */
			msg_len = message_process_cmd(i2c_xmit_mode[I2C2],
						  out_msg, sizeof(out_msg));
			if (msg_len > 0) {
				i2c_write_raw(I2C2, out_msg, msg_len);
			} else {
				CPRINTF("%s: unexpected mode %u\n",
						__func__, i2c_xmit_mode[I2C2]);
			}

			/* reset EC mode to NOP after transfer is finished */
			i2c_xmit_mode[I2C2] = CMDC_NOP;
		}
	}
}

static void i2c_event_handler(int port)
{

	/* save and clear status */
	i2c_sr1[port] = STM32_I2C_SR1(port);
	STM32_I2C_SR1(port) = 0;

	/* transfer matched our slave address */
	if (i2c_sr1[port] & (1 << 1)) {
		/* cleared by reading SR1 followed by reading SR2 */
		STM32_I2C_SR1(port);
		STM32_I2C_SR2(port);
#ifdef CONFIG_DEBUG
		CPRINTF("%s: ADDR\n", __func__);
#endif
	} else if (i2c_sr1[port] & (1 << 2)) {
		;
#ifdef CONFIG_DEBUG
		CPRINTF("%s: BTF\n", __func__);
#endif
	} else if (i2c_sr1[port] & (1 << 4)) {
		/* clear STOPF bit by reading SR1 and then writing CR1 */
		STM32_I2C_SR1(port);
		STM32_I2C_CR1(port) = STM32_I2C_CR1(port);
#ifdef CONFIG_DEBUG
		CPRINTF("%s: STOPF\n", __func__);
#endif
	} else {
		;
#ifdef CONFIG_DEBUG
		CPRINTF("%s: unknown event\n", __func__);
#endif
	}

	/* RxNE or TxE, wake the worker task */
	if (i2c_sr1[port] & ((1 << 6) | (1 << 7))) {
		if (port == I2C2)
			task_wake(TASK_ID_I2C2_WORK);
	}
}
static void i2c2_event_interrupt(void) { i2c_event_handler(I2C2); }
DECLARE_IRQ(STM32_IRQ_I2C2_EV, i2c2_event_interrupt, 3);

static void i2c_error_handler(int port)
{
	i2c_sr1[port] = STM32_I2C_SR1(port);

#ifdef CONFIG_DEBUG
	if (i2c_sr1[port] & 1 << 10) {
		/* ACK failed (NACK); expected when AP reads final byte.
		 * Software must clear AF bit. */
		CPRINTF("%s: AF detected\n", __func__);
	}
	CPRINTF("%s: tx byte count: %u, rx_byte_count: %u\n",
			__func__, tx_byte_count, rx_byte_count);
	CPRINTF("%s: I2C_SR1(%s): 0x%04x\n", __func__, port, i2c_sr1[port]);
	CPRINTF("%s: I2C_SR2(%s): 0x%04x\n",
			__func__, port, STM32_I2C_SR2(port));
#endif

	STM32_I2C_SR1(port) &= ~0xdf00;
}
static void i2c2_error_interrupt(void) { i2c_error_handler(I2C2); }
DECLARE_IRQ(STM32_IRQ_I2C2_ER, i2c2_error_interrupt, 2);

static int i2c_init2(void)
{
	int i;

	/* enable I2C2 clock */
	STM32_RCC_APB1ENR |= 1 << 22;

	/* set clock configuration : standard mode (100kHz) */
	STM32_I2C_CCR(I2C2) = I2C_CCR;

	/* set slave address */
	STM32_I2C_OAR1(I2C2) = I2C_ADDRESS;

	/* configuration : I2C mode / Periphal enabled, ACK enabled */
	STM32_I2C_CR1(I2C2) = (1 << 10) | (1 << 0);
	/* error and event interrupts enabled / input clock is 16Mhz */
	STM32_I2C_CR2(I2C2) = (1 << 9) | (1 << 8) | 0x10;

	/* clear status */
	STM32_I2C_SR1(I2C2) = 0;

	/* No tasks are waiting on ports */
	for (i = 0; i < NUM_PORTS; i++)
		task_waiting_on_port[i] = TASK_ID_INVALID;

	/* enable event and error interrupts */
	task_enable_irq(STM32_IRQ_I2C2_EV);
	task_enable_irq(STM32_IRQ_I2C2_ER);

	CPUTS("done\n");
	return EC_SUCCESS;
}

static int i2c_init1(void)
{
	/* enable clock */
	STM32_RCC_APB1ENR |= 1 << 21;

	/* set clock configuration : standard mode (100kHz) */
	STM32_I2C_CCR(I2C1) = I2C_CCR;

	/* configuration : I2C mode / Periphal enabled */
	STM32_I2C_CR1(I2C1) = (1 << 0);
	/* error and event interrupts enabled / input clock is 16Mhz */
	STM32_I2C_CR2(I2C1) = (1 << 9) | (1 << 8) | 0x10;

	/* clear status */
	STM32_I2C_SR1(I2C1) = 0;

	/* enable event and error interrupts */
	task_enable_irq(STM32_IRQ_I2C1_EV);
	task_enable_irq(STM32_IRQ_I2C1_ER);

	return EC_SUCCESS;

}

static int i2c_init(void)
{
	int rc = 0;

	/* FIXME: Add #defines to determine which channels to init */
	rc |= i2c_init2();
	rc |= i2c_init1();

	return rc;
}
DECLARE_HOOK(HOOK_INIT, i2c_init, HOOK_PRIO_DEFAULT);


/*****************************************************************************/
/* STM32 Host I2C */

#define SR1_SB		(1 << 0)	/* Start bit sent */
#define SR1_ADDR	(1 << 1)	/* Address sent */
#define SR1_BTF		(1 << 2)	/* Byte transfered */
#define SR1_ADD10	(1 << 3)	/* 10bit address sent */
#define SR1_STOPF	(1 << 4)	/* Stop detected */
#define SR1_RxNE	(1 << 6)	/* Data reg not empty */
#define SR1_TxE		(1 << 7)	/* Data reg empty */
#define SR1_BERR	(1 << 8)	/* Buss error */
#define SR1_ARLO	(1 << 9)	/* Arbitration lost */
#define SR1_AF		(1 << 10)	/* Ack failure */
#define SR1_OVR		(1 << 11)	/* Overrun/underrun */
#define SR1_PECERR	(1 << 12)	/* PEC err in reception */
#define SR1_TIMEOUT	(1 << 14)	/* Timeout : 25ms */

static inline void unused_var(int unused) {}

static inline void disable_i2c_interrupt(int port)
{
	STM32_I2C_CR2(port) &= ~(3 << 8);
}

static inline void enable_i2c_interrupt(int port)
{
	STM32_I2C_CR2(port) |= 3 << 8;
}

static inline void enable_ack(int port)
{
	STM32_I2C_CR1(port) |= (1 << 10);
}

static inline void disable_ack(int port)
{
	STM32_I2C_CR1(port) &= ~(1 << 10);
}

static inline void dump_i2c_reg(int port)
{
#ifdef CONFIG_DEBUG
	CPRINTF("CR1  : %016b\n", STM32_I2C_CR1(port));
	CPRINTF("CR2  : %016b\n", STM32_I2C_CR2(port));
	CPRINTF("SR1  : %016b\n", STM32_I2C_SR1(port));
	CPRINTF("SR2  : %016b\n", STM32_I2C_SR2(port));
	CPRINTF("OAR1 : %016b\n", STM32_I2C_OAR1(port));
	CPRINTF("OAR2 : %016b\n", STM32_I2C_OAR2(port));
	CPRINTF("DR   : %016b\n", STM32_I2C_DR(port));
	CPRINTF("CCR  : %016b\n", STM32_I2C_CCR(port));
	CPRINTF("TRISE: %016b\n", STM32_I2C_TRISE(port));
#endif /* CONFIG_DEBUG */
}

static void handle_i2c_error(int port, int rv)
{
	timestamp_t t1, t2;
	uint32_t r;

	if (rv)
		dump_i2c_reg(port);

	/* Clear rc_w0 bits */
	STM32_I2C_SR1(port) = 0;
	/* Clear seq read status bits */
	r = STM32_I2C_SR1(port);
	r = STM32_I2C_SR2(port);
	/* Clear busy state */
	t1 = get_time();
	while (STM32_I2C_SR2(port) & 2) {
		t2 = get_time();
		if (t2.val - t1.val > 1000000) {
			dump_i2c_reg(port);
			return;
		}
		/* Send stop */
		STM32_I2C_CR1(port) |= 1 << 9;
		usleep(10000);
	}
	unused_var(r);
}

static inline uint32_t read_clear_status(int port)
{
	uint32_t sr1, sr2;

	sr1 = STM32_I2C_SR1(port);
	sr2 = STM32_I2C_SR2(port);
	return (sr2 << 16) | (sr1 & 0xffff);
}

static int wait_status(int port, uint32_t mask)
{
	uint32_t r;
	timestamp_t t1, t2;


	t1 = get_time();
	r = STM32_I2C_SR1(port);
	while (mask ? ((r & mask) != mask) : r) {
		t2 = get_time();
		if (t2.val - t1.val > 100000) {
			CPRINTF(" m %016b\n", mask);
			CPRINTF(" - %016b\n", r);
			return EC_ERROR_TIMEOUT;
		}
		usleep(2000);
		r = STM32_I2C_SR1(port);
	}

	return EC_SUCCESS;
}

static int master_start(int port, int slave_addr)
{
	int rv;

	/* Change to master send mode, send start bit */
	STM32_I2C_CR1(port) |= (1 << 8);
	/* Wait for start bit sent event */
	rv = wait_status(port, SR1_SB);
	if (rv)
		return rv;
	/* Send address */
	STM32_I2C_DR(port) = slave_addr;
	/* Wait for addr ready */
	rv = wait_status(port, SR1_ADDR);
	if (rv)
		return rv;
	read_clear_status(port);

	return EC_SUCCESS;
}

static void master_stop(int port)
{
	STM32_I2C_CR1(port) |= (1 << 9);
}

static int i2c_transmit(int port, int slave_addr, uint8_t *data, int size,
	int stop)
{
	int rv, i;

	disable_ack(port);
	rv = master_start(port, slave_addr);
	if (rv)
		return rv;

	for (i = 0; i < size; i++) {
		rv = wait_status(port, SR1_TxE);
		if (rv)
			return rv;
		STM32_I2C_DR(port) = data[i];
	}

	rv = wait_status(port, SR1_BTF | SR1_TxE);
	if (rv)
		return rv;

	if (stop)
		master_stop(port);

	return EC_SUCCESS;
}

static int i2c_receive(int port, int slave_addr, uint8_t *data, int size)
{
	/* Master receiver sequence
	 *
	 * 1 byte
	 *   S   ADDR   ACK   D0   NACK   P
	 *  -o- -oooo- -iii- -ii- -oooo- -o-
	 *
	 * multi bytes
	 *   S   ADDR   ACK   D0   ACK   Dn-2   ACK   Dn-1   NACK   P
	 *  -o- -oooo- -iii- -ii- -ooo- -iiii- -ooo- -iiii- -oooo- -o-
	 *
	 */
	int rv, i;

	if (data == NULL || size < 1)
		return EC_ERROR_INVAL;

	/* Set ACK to high only on receiving more than 1 byte */
	if (size > 1)
		enable_ack(port);
	else
		disable_ack(port);


	/* Send START pulse, slave address, receive mode */
	rv = master_start(port, slave_addr | 1);
	if (rv)
		return rv;

	if (size >= 2) {
		for (i = 0; i < (size - 2); i++) {
			rv = wait_status(port, SR1_RxNE);
			if (rv)
				return rv;

			data[i] = STM32_I2C_DR(port);
		}

		/* Process last two bytes: data[n-2], data[n-1]
		 *   => wait rx ready
		 *   => [n-2] in data-reg, [n-1] in shift-reg
		 *   => [n-2] ACK done
		 *   => disable ACK to let [n-1] byte send NACK
		 *   => set STOP high
		 *   => read [n-2]
		 *   => wait rx ready
		 *   => read [n-1]
		 */
		rv = wait_status(port, SR1_RxNE);
		if (rv)
			return rv;

		disable_ack(port);
		master_stop(port);

		data[i] = STM32_I2C_DR(port);

		rv = wait_status(port, SR1_RxNE);
		if (rv)
			return rv;

		i++;
		data[i] = STM32_I2C_DR(port);
	} else {
		master_stop(port);
		rv = wait_status(port, SR1_RxNE);
		if (rv)
			return rv;
		data[0] = STM32_I2C_DR(port);
	}

	return EC_SUCCESS;
}

int i2c_read16(int port, int slave_addr, int offset, int *data)
{
	int rv;
	uint8_t reg, buf[2] = {0, 0};

	reg = offset & 0xff;

	mutex_lock(port_mutex + port);
	disable_i2c_interrupt(port);

	rv = i2c_transmit(port, slave_addr, &reg, 1, 0);
	if (!rv)
		rv = i2c_receive(port, slave_addr, buf, 2);
	handle_i2c_error(port, rv);

	enable_i2c_interrupt(port);
	mutex_unlock(port_mutex + port);

	if (rv)
		return rv;

	*data = ((int)buf[1] << 8) | buf[0];

	return EC_SUCCESS;
}

int i2c_write16(int port, int slave_addr, int offset, int data)
{
	int rv;
	uint8_t buf[3];

	buf[0] = offset & 0xff;
	buf[1] = data & 0xff;
	buf[2] = (data >> 8) & 0xff;

	mutex_lock(port_mutex + port);
	disable_i2c_interrupt(port);

	rv = i2c_transmit(port, slave_addr, buf, 3, 1);
	handle_i2c_error(port, rv);

	enable_i2c_interrupt(port);
	mutex_unlock(port_mutex + port);

	return rv;
}

int i2c_read8(int port, int slave_addr, int offset, int *data)
{
	int rv;
	uint8_t reg, buf;

	reg = offset & 0xff;
	mutex_lock(port_mutex + port);
	disable_i2c_interrupt(port);

	rv = i2c_transmit(port, slave_addr, &reg, 1, 0);
	if (!rv)
		rv = i2c_receive(port, slave_addr, &buf, 1);
	handle_i2c_error(port, rv);

	enable_i2c_interrupt(port);
	mutex_unlock(port_mutex + port);

	if (rv)
		return rv;

	*data = buf;

	return EC_SUCCESS;
}

int i2c_write8(int port, int slave_addr, int offset, int data)
{
	int rv;
	uint8_t buf[2];

	buf[0] = offset & 0xff;
	buf[1] = data & 0xff;

	mutex_lock(port_mutex + port);
	disable_i2c_interrupt(port);

	rv = i2c_transmit(port, slave_addr, buf, 2, 1);
	handle_i2c_error(port, rv);

	enable_i2c_interrupt(port);
	mutex_unlock(port_mutex + port);

	return rv;
}

int i2c_read_string(int port, int slave_addr, int offset, uint8_t *data,
	int len)
{
	/* TODO: implement i2c_read_block and i2c_read_string */

	if (len && data)
		*data = 0;

	return EC_SUCCESS;
}

/*****************************************************************************/
/* Console commands */

static int command_i2c(int argc, char **argv)
{
	int rw = 0;
	int slave_addr, offset;
	int value = 0;
	char *e;
	int rv = 0;

	if (argc < 4) {
		ccputs("Usage: i2c <r/r16> <slave_addr> <offset>\n");
		ccputs("Usage: i2c <w/w16> <slave_addr> <offset> <value>\n");
		return EC_ERROR_UNKNOWN;
	}

	if (strcasecmp(argv[1], "r") == 0) {
		rw = 0;
	} else if (strcasecmp(argv[1], "r16") == 0) {
		rw = 1;
	} else if (strcasecmp(argv[1], "w") == 0) {
		rw = 2;
	} else if (strcasecmp(argv[1], "w16") == 0) {
		rw = 3;
	} else {
		ccputs("Invalid rw mode : r / w / r16 / w16\n");
		return EC_ERROR_INVAL;
	}

	slave_addr = strtoi(argv[2], &e, 0);
	if (*e) {
		ccputs("Invalid slave_addr\n");
		return EC_ERROR_INVAL;
	}

	offset = strtoi(argv[3], &e, 0);
	if (*e) {
		ccputs("Invalid addr\n");
		return EC_ERROR_INVAL;
	}

	if (rw > 1) {
		if (argc < 5) {
			ccputs("No write value\n");
			return EC_ERROR_INVAL;
		}
		value = strtoi(argv[4], &e, 0);
		if (*e) {
			ccputs("Invalid write value\n");
			return EC_ERROR_INVAL;
		}
	}


	switch (rw) {
	case 0:
		rv = i2c_read8(I2C2, slave_addr, offset, &value);
		break;
	case 1:
		rv = i2c_read16(I2C2, slave_addr, offset, &value);
		break;
	case 2:
		rv = i2c_write8(I2C2, slave_addr, offset, value);
		break;
	case 3:
		rv = i2c_write16(I2C2, slave_addr, offset, value);
		break;
	}


	if (rv) {
		ccprintf("i2c command failed\n", rv);
		return rv;
	}

	if (rw == 0)
		ccprintf("0x%02x [%d]\n", value);
	else if (rw == 1)
		ccprintf("0x%04x [%d]\n", value);

	ccputs("ok\n");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(i2c, command_i2c);

#ifdef CONFIG_DEBUG
static int command_iic(int argc, char **argv)
{
	char *e;
	int write = 0;
	int value = 0;
	int port = I2C2;

	if (argc < 2) {
		ccputs("Usage: iic <reg_name> [write_value]\n");
		ccputs("  regs - cr1 cr2 oar1 oar2 dr sr1 sr2 ccr trise\n");
		return EC_ERROR_UNKNOWN;
	}

	if (argc == 3) {
		write = 1;
		value = strtoi(argv[2], &e, 0);
		if (*e) {
			ccputs("Invalid write value\n");
			return EC_ERROR_INVAL;
		}
	}

	if (strcasecmp(argv[1], "cr1") == 0) {
		if (write)
			STM32_I2C_CR1(port) = value;
		else
			value = STM32_I2C_CR1(port);
	} else if (strcasecmp(argv[1], "cr2") == 0) {
		if (write)
			STM32_I2C_CR2(port) = value;
		else
			value = STM32_I2C_CR2(port);
	} else if (strcasecmp(argv[1], "oar1") == 0) {
		if (write)
			STM32_I2C_OAR1(port) = value;
		else
			value = STM32_I2C_OAR2(port);
	} else if (strcasecmp(argv[1], "oar2") == 0) {
		if (write)
			STM32_I2C_OAR2(port) = value;
		else
			value = STM32_I2C_OAR2(port);
	} else if (strcasecmp(argv[1], "dr") == 0) {
		if (write)
			STM32_I2C_DR(port) = value;
		else
			value = STM32_I2C_DR(port);
	} else if (strcasecmp(argv[1], "sr1") == 0) {
		if (write)
			STM32_I2C_SR1(port) = value;
		else
			value = STM32_I2C_SR1(port);
	} else if (strcasecmp(argv[1], "sr2") == 0) {
		if (write)
			STM32_I2C_SR2(port) = value;
		else
			value = STM32_I2C_SR2(port);
	} else if (strcasecmp(argv[1], "ccr") == 0) {
		if (write)
			STM32_I2C_CCR(port) = value;
		else
			value = STM32_I2C_CCR(port);
	} else if (strcasecmp(argv[1], "trise") == 0) {
		if (write)
			STM32_I2C_TRISE(port) = value;
		else
			value = STM32_I2C_TRISE(port);
	} else {
		ccprintf("Invalid register %s\n", argv[1]);
		return EC_ERROR_INVAL;
	}

	if (!write)
		ccprintf("%016b [%04x]\n", value, value);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(iic, command_iic);
#endif /* CONFIG_DEBUG */

