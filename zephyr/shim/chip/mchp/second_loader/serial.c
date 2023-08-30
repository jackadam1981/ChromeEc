/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "MCHP_MEC172x.h"
#include "common.h"
#include "gpio.h"
#include "serial.h"

#define LENGTH_8_BIT 0x03
#define ONE_STOP_BIT 0x00

enum BAUD_RATE {
	RATE_134 = 857,
	RATE_1200 = 96,
	RATE_3600 = 32,
	RATE_9600 = 12,
	RATE_19200 = 6,
	RATE_57600 = 2,
	RATE_115200 = 1
};

/*
 * Initialize UART pins, Baudrate
 * - MMCR_8b(UART_LINE_CONTROL)  = 0x83;             // N 8 1
 * - MMCR_8b(UART_DIVISOR_LATCH_1)=BaudRate[idx];    // BaudRateDiv.
 * - MMCR_8b(UART_LINE_CONTROL)  = 0x03;             // Clr DLAB
 */
void serial_init(void)
{
	/* GPIO_MUX_FUNC1 TXD(UART0_TX) */
	gpio_pin_ctrl1_reg_write(0104, 0x001000UL);
	/* GPIO_MUX_FUNC1 RXD(UART0_RX) */
	gpio_pin_ctrl1_reg_write(0105, 0x001000UL);

	/* Init the host i/f UART0 block */
	HOST_IF_UART->FIFO_CR_b.CLEAR_RECV_FIFO = 1;
	HOST_IF_UART->FIFO_CR_b.CLEAR_XMIT_FIFO = 1;
	HOST_IF_UART->FIFO_CR_b.RECV_FIFO_TRIGGER_LEVEL = 0;
	HOST_IF_UART->FIFO_CR_b.EXRF = 1;

	/* RST by VCC1_RESET */
	HOST_IF_UART->CONFIG = 0;
	HOST_IF_UART->LINE_CR_b.DLAB = 1;
	HOST_IF_UART->BAUDRATE_LSB = RATE_57600;
	HOST_IF_UART->BAUDRATE_MSB = 0;
	HOST_IF_UART->LINE_CR_b.DLAB = 0;
	HOST_IF_UART->LINE_CR_b.STOP_BITS = ONE_STOP_BIT;
	HOST_IF_UART->LINE_CR_b.WORD_LENGTH = LENGTH_8_BIT;
	/* MCR_OUT2 */
	HOST_IF_UART->MODEM_CR = 0x08;
	HOST_IF_UART->ACTIVATE = 1;
}

int serial_send_host_char(int c)
{
	while (HOST_IF_UART->LINE_STS_b.TRANSMIT_EMPTY == 0)
		;
	HOST_IF_UART->TX_DATA = (unsigned char)c;
	while (HOST_IF_UART->LINE_STS_b.TRANSMIT_EMPTY)
		;
	return c;
}

bool serial_receive_host_char(uint8_t *rx_data)
{
	if (HOST_IF_UART->LINE_STS & UART0_STS_DATA_RDY_Msk) {
		*rx_data = HOST_IF_UART->RX_DATA;
		return true;
	}
	return false;
}
