/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MAX32660 Console UART Module for Chrome EC */

#include <stdint.h>
#include "system.h"
#include "task.h"
#include "uart.h"
#include "registers.h"
#include "tmr_regs.h"
#include "gpio_regs.h"
#include "gpio_api.h"
#include "gcr_regs.h"
#include "uart_regs.h"
#include "mxc_errors.h"

static int done_uart_init_yet;

#ifndef UARTN
#define UARTN CONFIG_UART_HOST
#endif

#define EC_UART_IRQn EC_UART1_IRQn

#define UART_BAUD 115200

#define UART_ER_IF                                                             \
	(MXC_F_UART_INT_FL_RX_FRAME_ERROR |                                    \
	 MXC_F_UART_INT_FL_RX_PARITY_ERROR | MXC_F_UART_INT_FL_RX_OVERRUN)

#define UART_ER_IE                                                             \
	(MXC_F_UART_INT_EN_RX_FRAME_ERROR |                                    \
	 MXC_F_UART_INT_EN_RX_PARITY_ERROR | MXC_F_UART_INT_EN_RX_OVERRUN)

#define UART_RX_IF (UART_ER_IF | MXC_F_UART_INT_FL_RX_FIFO_THRESH)

#define UART_RX_IE (UART_ER_IE | MXC_F_UART_INT_EN_RX_FIFO_THRESH)

#define UART_TX_IF                                                             \
	(UART_ER_IF | MXC_F_UART_INT_FL_TX_FIFO_ALMOST_EMPTY |                 \
	 MXC_F_UART_INT_FL_TX_FIFO_THRESH)

#define UART_TX_IE                                                             \
	(UART_ER_IE | MXC_F_UART_INT_EN_TX_FIFO_ALMOST_EMPTY |                 \
	 MXC_F_UART_INT_EN_TX_FIFO_THRESH)

#define UART_RX_THRESHOLD_LEVEL 1

/**
 * @brief      Alternate clock rate. (7.3728MHz) */
#define UART_ALTERNATE_CLOCK_HZ 7372800

/// \brief Parity settings type
typedef enum {
	UART_PARITY_DISABLE = 0, /**< Parity Disabled */

	UART_PARITY_EVEN_0 =
		(MXC_F_UART_CTRL_PARITY_EN | MXC_S_UART_CTRL_PARITY_EVEN |
		 MXC_F_UART_CTRL_PARMD), /**< Use for Even Parity 0 */

	UART_PARITY_EVEN_1 =
		(MXC_F_UART_CTRL_PARITY_EN |
		 MXC_S_UART_CTRL_PARITY_EVEN), /**< Use for Even Parity 1 */

	UART_PARITY_EVEN = UART_PARITY_EVEN_1, /** Conventional even parity */

	UART_PARITY_ODD_0 =
		(MXC_F_UART_CTRL_PARITY_EN | MXC_S_UART_CTRL_PARITY_ODD |
		 MXC_F_UART_CTRL_PARMD), /**< Use for Odd Parity 0 */

	UART_PARITY_ODD_1 =
		(MXC_F_UART_CTRL_PARITY_EN |
		 MXC_S_UART_CTRL_PARITY_ODD), /**< Use for Odd Parity 1 */

	UART_PARITY_ODD = UART_PARITY_ODD_1, /** Conventional odd parity */

	UART_PARITY_MARK_0 =
		(MXC_F_UART_CTRL_PARITY_EN | MXC_S_UART_CTRL_PARITY_MARK |
		 MXC_F_UART_CTRL_PARMD), /**< Use for Mark Parity 0 */

	UART_PARITY_MARK_1 =
		(MXC_F_UART_CTRL_PARITY_EN |
		 MXC_S_UART_CTRL_PARITY_MARK), /**< Use for Mark Parity 1 */

	UART_PARITY_MARK = UART_PARITY_MARK_1, /** Conventional mark parity */

	UART_PARITY_SPACE_0 =
		(MXC_F_UART_CTRL_PARITY_EN | MXC_S_UART_CTRL_PARITY_SPACE |
		 MXC_F_UART_CTRL_PARMD), /**< Use for Space Parity 0 */

	UART_PARITY_SPACE_1 =
		(MXC_F_UART_CTRL_PARITY_EN |
		 MXC_S_UART_CTRL_PARITY_SPACE), /**< Use for Space Parity 1 */

	UART_PARITY_SPACE =
		UART_PARITY_SPACE_1, /** Conventional space parity */

} uart_parity_t;

/** @brief Map control */
typedef enum {
	MAP_A,
	MAP_B,
	MAP_C,
} sys_map_t;

typedef enum {
	UART_FLOW_DISABLE,
	UART_FLOW_ENABLE,
} sys_uart_flow_t;

/** @brief UART system configuration object */
typedef struct {
	sys_map_t map;
	sys_uart_flow_t flow_flag;
} sys_cfg_uart_t;

/**
 * @brief      Message size settings */
typedef enum {
	UART_DATA_SIZE_5_BITS =
		MXC_S_UART_CTRL_CHAR_SIZE_5, /**< Data Size 5 Bits */
	UART_DATA_SIZE_6_BITS =
		MXC_S_UART_CTRL_CHAR_SIZE_6, /**< Data Size 6 Bits */
	UART_DATA_SIZE_7_BITS =
		MXC_S_UART_CTRL_CHAR_SIZE_7, /**< Data Size 7 Bits */
	UART_DATA_SIZE_8_BITS =
		MXC_S_UART_CTRL_CHAR_SIZE_8, /**< Data Size 8 Bits */
} uart_size_t;

/**
 * @brief      Stop bit settings */
typedef enum {
	UART_STOP_1 = 0, /**< UART Stop 1 clock cycle */
	UART_STOP_1P5 =
		MXC_F_UART_CTRL_STOPBITS, /**< UART Stop 1.5 clock cycle */
	UART_STOP_2 = MXC_F_UART_CTRL_STOPBITS, /**< UART Stop 2 clock cycle */
} uart_stop_t;

/**
 * @brief      Flow control */
typedef enum {
	UART_FLOW_CTRL_DIS = 0, /**< RTS CTS flow is disabled */
	UART_FLOW_CTRL_EN =
		MXC_F_UART_CTRL_FLOW_CTRL, /**< RTS CTS flow is enabled */
} uart_flow_ctrl_t;

/**
 * @brief      Flow control Polarity */
typedef enum {
	UART_FLOW_POL_DIS = 0,
	UART_FLOW_POL_EN = MXC_F_UART_CTRL_FLOW_POL,
} uart_flow_pol_t;

/**
 * @brief      UART configuration type. */
typedef struct {
	uart_parity_t parity; /**        Configure parity checking */
	uart_size_t size;     /**        Configure character size */
	uart_stop_t stop; /**        Configure the number of stop bits to use */
	uart_flow_ctrl_t flow; /**        Configure hardware flow control */
	uart_flow_pol_t pol;   /**        Configure hardware flow control */
	uint32_t baud;	 /**        Configure baud rate */
} uart_cfg_t;

const gpio_cfg_t gpio_cfg_uart0rtscts = {PORT_0, (PIN_6 | PIN_7),
					 GPIO_FUNC_ALT2, GPIO_PAD_NONE};
const gpio_cfg_t gpio_cfg_uart0a = {PORT_0, (PIN_4 | PIN_5), GPIO_FUNC_ALT2,
				    GPIO_PAD_NONE};
const gpio_cfg_t gpio_cfg_uart1rtscts = {PORT_0, (PIN_12 | PIN_13),
					 GPIO_FUNC_ALT2, GPIO_PAD_NONE};
const gpio_cfg_t gpio_cfg_uart1a = {PORT_0, (PIN_10 | PIN_11), GPIO_FUNC_ALT2,
				    GPIO_PAD_NONE};
const gpio_cfg_t gpio_cfg_uart1b = {PORT_0, (PIN_0 | PIN_1), GPIO_FUNC_ALT3,
				    GPIO_PAD_NONE};
const gpio_cfg_t gpio_cfg_uart1c = {PORT_0, (PIN_6 | PIN_7), GPIO_FUNC_ALT3,
				    GPIO_PAD_NONE};

int uart_gpio_configure(const gpio_cfg_t *cfg)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(cfg->port);

	// Set the GPIO type
	switch (cfg->func) {
	case GPIO_FUNC_IN:
		gpio->out_en_clr = cfg->mask;
		gpio->en_set = cfg->mask;
		gpio->en1_clr = cfg->mask;
		gpio->en2_clr = cfg->mask;
		break;
	case GPIO_FUNC_OUT:
		gpio->out_en_set = cfg->mask;
		gpio->en_set = cfg->mask;
		gpio->en1_clr = cfg->mask;
		gpio->en2_clr = cfg->mask;
		break;
	case GPIO_FUNC_ALT1:
		gpio->en_clr = cfg->mask;
		gpio->en1_clr = cfg->mask;
		gpio->en2_clr = cfg->mask;
		break;
	case GPIO_FUNC_ALT2:
		gpio->en_clr = cfg->mask;
		gpio->en1_set = cfg->mask;
		gpio->en2_clr = cfg->mask;
		break;
	case GPIO_FUNC_ALT3:
		gpio->en_set = cfg->mask;
		gpio->en1_set = cfg->mask;
		break;
	case GPIO_FUNC_ALT4:
		gpio->en_clr = cfg->mask;
		gpio->en1_set = cfg->mask;
		gpio->en2_set = cfg->mask;
		break;
	default:
		return E_BAD_PARAM;
	}

	// Configure the pad
	switch (cfg->pad) {
	case GPIO_PAD_NONE:
		gpio->pad_cfg1 &= ~cfg->mask;
		gpio->pad_cfg2 &= ~cfg->mask;
		gpio->ps &= ~cfg->mask;
		break;
	case GPIO_PAD_PULL_UP:
		gpio->pad_cfg1 |= cfg->mask;
		gpio->pad_cfg2 &= ~cfg->mask;
		gpio->ps |= cfg->mask;
		break;
	case GPIO_PAD_PULL_DOWN:
		gpio->pad_cfg1 &= ~cfg->mask;
		gpio->pad_cfg2 |= cfg->mask;
		gpio->ps &= ~cfg->mask;
		break;
	default:
		return E_BAD_PARAM;
	}

	return E_NO_ERROR;
}

/* ************************************************************************ */
int uart_configure(mxc_uart_regs_t *uart, const sys_cfg_uart_t *sys_cfg)
{
	// Configure GPIO for UART
	if (uart == MXC_UART0) {
		// SYS_ClockEnable(SYS_PERIPH_CLOCK_UART0);

		// MXC_F_GCR_PERCKCN0_UART0D,
		// MXC_F_GCR_PERCKCN0_UART1D,
		MXC_GCR->perckcn0 &= ~(MXC_F_GCR_PERCKCN0_UART1D);

		if (sys_cfg->map == MAP_A) {
			uart_gpio_configure(&gpio_cfg_uart0a);
		} else {
			return E_BAD_PARAM;
		}
		if (sys_cfg->flow_flag == UART_FLOW_ENABLE) {
			uart_gpio_configure(&gpio_cfg_uart0rtscts);
		}
	}
	if (uart == MXC_UART1) {
		// SYS_ClockEnable(SYS_PERIPH_CLOCK_UART1);
		MXC_GCR->perckcn0 &= ~(MXC_F_GCR_PERCKCN0_UART1D);

		if (sys_cfg->map == MAP_A) {
			uart_gpio_configure(&gpio_cfg_uart1a);
		} else if (sys_cfg->map == MAP_B) {
			uart_gpio_configure(&gpio_cfg_uart1b);
		} else if (sys_cfg->map == MAP_C) {
			uart_gpio_configure(&gpio_cfg_uart1c);
		} else {
			return E_BAD_PARAM;
		}
		if (sys_cfg->flow_flag == UART_FLOW_ENABLE) {
			uart_gpio_configure(&gpio_cfg_uart1rtscts);
		}
	}
	return E_NO_ERROR;
}

/* ************************************************************************* */
int uart_device_init(mxc_uart_regs_t *uart, const uart_cfg_t *cfg,
		     const sys_cfg_uart_t *sys_cfg)
{
	int err;
	int uart_num;

	uint32_t baud0 = 0, baud1 = 0, div;
	int32_t factor = -1;

	// Get the state array index
	uart_num = MXC_UART_GET_IDX(uart);
	if (uart_num == -1) {
		return E_BAD_PARAM;
	}

	if ((err = uart_configure(uart, sys_cfg)) != E_NO_ERROR) {
		return err;
	}

	// Drain FIFOs and enable UART and set configuration
	uart->ctrl = (MXC_F_UART_CTRL_ENABLE | cfg->parity | cfg->size |
		      cfg->stop | cfg->flow | cfg->pol);

	// Set the baud rate

	div = PeripheralClock / ((cfg->baud)); // constant part of DIV (i.e. DIV
					       // * (Baudrate*factor_int))

	do {
		factor += 1;
		baud0 = div >> (7 - factor); // divide by 128,64,32,16 to
					     // extract integer part
		baud1 = ((div << factor) -
			 (baud0 << 7)); // subtract factor corrected div -
					// integer parts

	} while ((baud0 == 0) && (factor < 4));

	uart->baud0 = ((factor << MXC_F_UART_BAUD0_FACTOR_POS) | baud0);
	uart->baud1 = baud1;

	return E_NO_ERROR;
}

/* ************************************************************************* */
unsigned uart_number_write_available(mxc_uart_regs_t *uart)
{
	return MXC_UART_FIFO_DEPTH -
	       ((uart->status & MXC_F_UART_STATUS_TX_FIFO_CNT) >>
		MXC_F_UART_STATUS_TX_FIFO_CNT_POS);
}

/* ************************************************************************* */
unsigned uart_number_read_available(mxc_uart_regs_t *uart)
{
	return ((uart->status & MXC_F_UART_STATUS_RX_FIFO_CNT) >>
		MXC_F_UART_STATUS_RX_FIFO_CNT_POS);
}

void uartn_init(int uart_num)
{
	int error;
	uint32_t flags;
	uart_cfg_t cfg;
	mxc_uart_regs_t *uart;
	const sys_cfg_uart_t sys_uart_cfg = {
		MAP_A,
		UART_FLOW_DISABLE,
	};

	uart = MXC_UART_GET_UART(uart_num);

	/* Initialize the UART */
	cfg.parity = UART_PARITY_DISABLE;
	cfg.size = UART_DATA_SIZE_8_BITS;
	cfg.stop = 0;
	cfg.flow = 0;
	cfg.pol = 1;
	cfg.baud = UART_BAUD;

	error = uart_device_init(uart, &cfg, &sys_uart_cfg);
	if (error != E_NO_ERROR) {
		while (1) {
		}
	}

	uart->thresh_ctrl = UART_RX_THRESHOLD_LEVEL
			    << MXC_F_UART_THRESH_CTRL_RX_FIFO_THRESH_POS;

	// Clear Interrupt Flags
	flags = uart->int_fl;
	uart->int_fl = flags;

	// Enable the RX interrupts
	uart->int_en |= UART_RX_IE;
}

void uartn_enable_tx_interrupt(int uart_num)
{
	// Enable the interrupts
	MXC_UART_GET_UART(uart_num)->int_en |= UART_TX_IE;
}

void uartn_disable_tx_interrupt(int uart_num)
{
	// Disable the interrupts
	MXC_UART_GET_UART(uart_num)->int_en &= ~UART_TX_IE;
}

void uartn_enable_rx_interrupt(int uart_num)
{
	// Enable the interrupts
	MXC_UART_GET_UART(uart_num)->int_en |= UART_RX_IE;
}

void uartn_disable_rx_interrupt(int uart_num)
{
	// Enable the interrupts
	MXC_UART_GET_UART(uart_num)->int_en &= ~UART_RX_IE;
}

int uartn_tx_in_progress(int uart_num)
{
	return ((MXC_UART_GET_UART(uart_num)->status &
		 (MXC_F_UART_STATUS_TX_BUSY)) != 0);
}

void uartn_tx_flush(int uart_num)
{
	while (uartn_tx_in_progress(uart_num)) {
	}
}

int uartn_tx_ready(int uart_num)
{
	int avail;
	avail = uart_number_write_available(MXC_UART_GET_UART(uart_num));
	/* True if the TX buffer is not completely full */
	return (avail != 0);
}

int uartn_rx_available(int uart_num)
{
	int avail;
	/* True if the RX buffer is not completely empty. */
	avail = uart_number_read_available(MXC_UART_GET_UART(uart_num));
	return (avail != 0);
}

void uartn_write_char(int uart_num, char c)
{
	int avail;
	mxc_uart_regs_t *uart;

	uart = MXC_UART_GET_UART(uart_num);
	// Refill the TX FIFO
	avail = uart_number_write_available(uart);

	// wait until there is room in the fifo
	while (avail == 0) {
		avail = uart_number_write_available(uart);
	}

	// stuff the fifo with the character
	uart->fifo = c;
}

int uartn_read_char(int uart_num)
{
	int c;
	c = MXC_UART_GET_UART(uart_num)->fifo;
	return c;
}

void uartn_clear_interrupt_flags(int uart_num)
{
	uint32_t flags;
	// Read and clear interrupts
	//    intst = MXC_UART_GET_UART(uart_num)->int_fl;
	//    MXC_UART_GET_UART(uart_num)->int_fl = ~intst;

	flags = MXC_UART_GET_UART(uart_num)->int_fl;
	MXC_UART_GET_UART(uart_num)->int_fl = flags;
}

static inline int uartn_is_rx_interrupt(int uart_num)
{
	return MXC_UART_GET_UART(uart_num)->int_fl & UART_RX_IF;
}

static inline int uartn_is_tx_interrupt(int uart_num)
{
	return MXC_UART_GET_UART(uart_num)->int_fl & UART_TX_IF;
}

int uart_init_done(void)
{
	return done_uart_init_yet;
}

void uart_tx_start(void)
{
	/* Do not allow deep sleep while transmit in progress */
	disable_sleep(SLEEP_MASK_UART);
	/*
	 * Re-enable the transmit interrupt, then forcibly trigger the
	 * interrupt.
	 */
	uartn_enable_tx_interrupt(UARTN);
	task_trigger_irq(EC_UART_IRQn);
}

void uart_tx_stop(void)
{
	uartn_disable_tx_interrupt(UARTN);
	/* Re-allow deep sleep */
	enable_sleep(SLEEP_MASK_UART);
}

int uart_tx_in_progress(void)
{
	return uartn_tx_in_progress(UARTN);
}

void uart_tx_flush(void)
{
	uartn_tx_flush(UARTN);
}

int uart_tx_ready(void)
{
	/* True if the TX buffer is not completely full */
	return uartn_tx_ready(UARTN);
}

int uart_rx_available(void)
{
	/* True if the RX buffer is not completely empty. */
	return uartn_rx_available(UARTN);
}

void uart_write_char(char c)
{
	/* write a character to the UART */
	uartn_write_char(UARTN, c);
}

int uart_read_char(void)
{
	return uartn_read_char(UARTN);
}

/**
 * Interrupt handlers for UART
 */
void uart_rxtx_interrupt(void)
{
	/* Process the Console Input */
	uart_process_input();
	/* Process the Buffered Console Output */
	uart_process_output();
	uartn_clear_interrupt_flags(UARTN);
}
DECLARE_IRQ(EC_UART_IRQn, uart_rxtx_interrupt, 1);

void uart_init(void)
{
	/* Initialize the Console UART */
	uartn_init(UARTN);
	/* Enable the IRQ */
	task_enable_irq(EC_UART_IRQn);
	/* Set a flag for the system that the UART has been initialized */
	done_uart_init_yet = 1;
}
