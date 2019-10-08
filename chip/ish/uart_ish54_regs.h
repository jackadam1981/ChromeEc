/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UART register defines for ISH5.4 */

#ifndef __CROS_EC_UART_ISH54_REGS_H_
#define __CROS_EC_UART_ISH54_REGS_H_


/*
 * RBR: Receive Buffer register     (BLAB bit = 0)
 */
#define UART_OFFSET_RBR      (0x00)

/*
 * THR: Transmit Holding register   (BLAB bit = 0)
 */
#define UART_OFFSET_THR      (0x00)

/*
 * DLL: Divisor Latch Reg. low byte  (BLAB bit = 1)
 * baud rate = (serial clock freq) / (16 * divisor)
 */
#define UART_OFFSET_DLL      (0x00)

/*
 * DLH: Divisor Latch Reg. high byte (BLAB bit = 1)
 */
#define UART_OFFSET_DLH      (0x04)

/*
 * IER: Interrupt Enable register   (BLAB bit = 0)
 */
#define UART_OFFSET_IER      (0x04)

#define IER_RECV        (0x01)  /* Receive Data Available, 2nd highest prio */
#define IER_TDRQ        (0x02)  /* Transmit Holding Register Empty, 3rd highest prio */
#define IER_LINE_STAT   (0x04)  /* Receiver Line Status, highest prio */
#define IER_MODEM       (0x08)  /* Modem Status, 4th highest prio */
#define IER_PTIME       (0x80)  /* Programmable THRE Interrupt Mode Enable */

/*
 * IIR: Interrupt ID register
 */
#define UART_OFFSET_IIR      (0x08)

#define IIR_MODEM           (0x00)  /* Prio: 4 */
#define IIR_NO_INTR         (0x01)
#define IIR_THRE            (0x02)  /* Prio: 3 */
#define IIR_RECV_DATA       (0x04)  /* Prio: 2 */
#define IIR_LINE_STAT       (0x06)  /* Prio: 1 */
#define IIR_BUSY            (0x07)  /* Prio: 5 */
#define IIR_TIME_OUT        (0x0C)  /* Prio: 2 */
#define IIR_SOURCE          (0x0F)


/*
 * FCR: FIFO Control register (FIFO_MODE != NONE)
 */
#define UART_OFFSET_FCR      (0x08)

#define FIFO_SIZE         64
#define FCR_FIFO_ENABLE         (0x01)
#define FCR_RESET_RX            (0x02)
#define FCR_RESET_TX            (0x04)
#define FCR_DMA_MODE            (0x08)

/*
 * LCR: Line Control register
 */
#define UART_OFFSET_LCR       (0x0c)

#define LCR_5BIT_CHR            (0x00)
#define LCR_6BIT_CHR            (0x01)
#define LCR_7BIT_CHR            (0x02)
#define LCR_8BIT_CHR            (0x03)
#define LCR_BIT_CHR_MASK        (0x03)

#define LCR_STOP                (1 << 2)  /* 0: 1 stop bit, 1: 1.5/2 */
#define LCR_PEN                 (1 << 3)  /* Parity Enable */
#define LCR_EPS                 (1 << 4)  /* Even Parity Select */
#define LCR_SP                  (1 << 5)  /* Stick Parity */
#define LCR_BC                  (1 << 6)  /* Break Control */
#define LCR_DLAB                (1 << 7)  /* Divisor Latch Access */

/*
 * MCR: Modem Control register
 */
#define UART_OFFSET_MCR       (0x10)
#define MCR_DTR                 (0x1)     /* Data terminal ready */
#define MCR_RTS                 (0x2)     /* Request to send */
#define MCR_LOOP                (0x10)    /* LoopBack bit*/

#define MCR_INTR_ENABLE         (0x08)    /* User-designated OUT2 */
#define MCR_AUTO_FLOW_EN        (0x20)

/*
 * LSR: Line Status register
 */
#define UART_OFFSET_LSR       (0x14)

#define LSR_DR                  (0x01)         /* Data Ready */
#define LSR_OE                  (0x02)         /* Overrun error */
#define LSR_PE                  (0x04)         /* Parity error */
#define LSR_FE                  (0x08)         /* Framing error */
#define LSR_BI                  (0x10)         /* Breaking interrupt */
#define LSR_TDRQ                (0x20)       /* Transmit Holding Register Empty */
#define LSR_TEMT                (0x40)       /* Transmitter empty */

/*
 * MSR: Modem Status register
 */
#define UART_OFFSET_MSR       (0x18)

#define MSR_CTS            (1 << 4) /* Clear To Send signal */

/*
 * TFL: Transmit FIFO Level
 */
#define UART_OFFSET_TFL       (0x80)

/*
 * RFL: Receive FIFO Level
 */
#define UART_OFFSET_RFL       (0x84)

#endif
