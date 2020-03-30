/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* SCP UART module registers */

#ifndef __CROS_EC_UART_REGS_H
#define __CROS_EC_UART_REGS_H

#include "registers.h"

/* DLAB (Divisor Latch Access Bit) == 0 */

/* (Read) receiver buffer register */
#define UART_RBR(n)			UART_REG(n, 0)
/* (Write) transmitter holding register */
#define UART_THR(n)			UART_REG(n, 0)
/* (Write) interrupt enable register */
#define UART_IER(n)			UART_REG(n, 1)
#define   UART_IER_RDI			BIT(0) /* received data */
#define   UART_IER_THRI			BIT(1) /* THR empty */
#define   UART_IER_RLSI			BIT(2) /* receiver LSR change */
#define   UART_IER_MSI			BIT(3) /* MSR change */
/* (Read) interrupt identification register */
#define UART_IIR(n)			UART_REG(n, 2)
#define   UART_IIR_NO_INT		BIT(0) /* no int pending */
#define   UART_IIR_ID_MASK		0x0e
#define   UART_IIR_MSI			0x00
#define   UART_IIR_THRI			0x02
#define   UART_IIR_RDI			0x04
#define   UART_IIR_RLSI			0x06
#define   UART_IIR_BUSY			0x07 /* DW APB busy? TODO: should be 0xc? (16550) */
/* (Write) FIFO control register */
#define UART_FCR(n)			UART_REG(n, 2)
#define   UART_FCR_ENABLE_FIFO		BIT(0) /* enable FIFO */
#define   UART_FCR_CLEAR_RCVR		BIT(1) /* clear receive FIFO */
#define   UART_FCR_CLEAR_XMIT		BIT(2) /* clear transmit FIFO */
#define   UART_FCR_DMA_SELECT		BIT(3) /* select DMA mode */
/* receive FIFO interrupt trigger level */
#define   UART_FCR_T_TRIG_00		0x00 /* 1 byte */
#define   UART_FCR_T_TRIG_01		0x10 /* 6 bytes */
#define   UART_FCR_T_TRIG_10		0x20 /* 12 bytes */
#define   UART_FCR_T_TRIG_11		0x30
#define   UART_FCR_R_TRIG_00		0x00 /* 1 byte */
#define   UART_FCR_R_TRIG_01		0x40 /* 4 bytes */
#define   UART_FCR_R_TRIG_10		0x80 /* 8 bytes */
#define   UART_FCR_R_TRIG_11		0x80 /* 14 bytes TODO: should be 0xc0? */
/* (Write) line control register */
#define UART_LCR(n)			UART_REG(n, 3)
#define   UART_LCR_WLEN5		0 /* word length 5 bits */
#define   UART_LCR_WLEN6		1
#define   UART_LCR_WLEN7		2
#define   UART_LCR_WLEN8		3
#define   UART_LCR_STOP			BIT(2) /* stop bits: 1bit, 2bits */
#define   UART_LCR_PARITY		BIT(3) /* parity enable */
#define   UART_LCR_EPAR			BIT(4) /* even parity */
#define   UART_LCR_SPAR			BIT(5) /* stick parity */
#define   UART_LCR_SBC			BIT(6) /* set break control */
#define   UART_LCR_DLAB			BIT(7) /* divisor latch access */
/* (Write) modem control register */
#define UART_MCR(n)			UART_REG(n, 4)
/* (Read) line status register */
#define UART_LSR(n)			UART_REG(n, 5)
#define   UART_LSR_DR			BIT(0) /* data ready */
#define   UART_LSR_OE			BIT(1) /* overrun error */
#define   UART_LSR_PE			BIT(2) /* parity error */
#define   UART_LSR_FE			BIT(3) /* frame error */
#define   UART_LSR_BI			BIT(4) /* break interrupt */
#define   UART_LSR_THRE			BIT(5) /* THR empty */
#define   UART_LSR_TEMT			BIT(6) /* THR empty, line idle */
#define   UART_LSR_FIFOE		BIT(7) /* FIFO error */
/* (Read) modem status register */
#define UART_MSR(n)			UART_REG(n, 6)
/* (Read/Write) scratch register */
#define UART_SCR(n)			UART_REG(n, 7)

/* DLAB == 1 */

/* (Write) divisor latch */
#define UART_DLL(n)			UART_REG(n, 0)
#define UART_DLH(n)			UART_REG(n, 1)

/* MTK extension */
#define UART_HIGHSPEED(n)		UART_REG(n, 9)

#endif /* __CROS_EC_UART_REGS_H */
