/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UART module for ISH 3.0 */

#ifndef __CROS_EC_UART_DEFS_H_
#define __CROS_EC_UART_DEFS_H_

#include <stdint.h>
#include <stddef.h>

#define	UART_ERROR		-1
#define UART_BUSY		-2
#define HSU_BASE		ISH_UART_BASE
#define UART0_OFFS              (0x80)
#define UART0_BASE              (ISH_UART_BASE + UART0_OFFS)
#define UART0_SIZE              (0x80)

#define UART1_OFFS              (0x100)
#define UART1_BASE              (ISH_UART_BASE + UART1_OFFS)
#define UART1_SIZE              (0x80)

#define UART2_OFFS              (0x180)
#define UART2_BASE              (ISH_UART_BASE + UART2_OFFS)
#define UART2_SIZE              (0x80)

#define HSU_DMA_OFFS            (0x400)
#define HSU_DMA_BASE            (HSU_BASE + HSU_DMA_OFFS)

/* Register accesses */
#define LSR(n)  (uart_ctx[n].base + UART_REG_LSR * uart_ctx[n].addr_interval)
#define THR(n)  (uart_ctx[n].base + UART_REG_THR * uart_ctx[n].addr_interval)
#define FOR(n)  (uart_ctx[n].base + UART_REG_FOR * uart_ctx[n].addr_interval)
#define RBR(n)  (uart_ctx[n].base + UART_REG_RBR * uart_ctx[n].addr_interval)
#define DLL(n)  (uart_ctx[n].base + UART_REG_DLL * uart_ctx[n].addr_interval)
#define DLH(n)  (uart_ctx[n].base + UART_REG_DLH * uart_ctx[n].addr_interval)
#define DLD(n)  (uart_ctx[n].base + UART_REG_DLD * uart_ctx[n].addr_interval)
#define IER(n)  (uart_ctx[n].base + UART_REG_IER * uart_ctx[n].addr_interval)
#define IIR(n)  (uart_ctx[n].base + UART_REG_IIR * uart_ctx[n].addr_interval)
#define FCR(n)  (uart_ctx[n].base + UART_REG_FCR * uart_ctx[n].addr_interval)
#define LCR(n)  (uart_ctx[n].base + UART_REG_LCR * uart_ctx[n].addr_interval)
#define MCR(n)  (uart_ctx[n].base + UART_REG_MCR * uart_ctx[n].addr_interval)
#define MSR(n)  (uart_ctx[n].base + UART_REG_MSR * uart_ctx[n].addr_interval)
#define FCTR(n)  (uart_ctx[n].base + UART_REG_FCTR * uart_ctx[n].addr_interval)
#define EFR(n)  (uart_ctx[n].base + UART_REG_EFR * uart_ctx[n].addr_interval)
#define RXTRG(n)  \
		(uart_ctx[n].base + UART_REG_RXTRG * uart_ctx[n].addr_interval)
#define ABR(n)  (uart_ctx[n].base + UART_REG_ABR * uart_ctx[n].addr_interval)
#define PS(n)  (uart_ctx[n].base + UART_REG_PS * uart_ctx[n].addr_interval)
#define MUL(n)  (uart_ctx[n].base + UART_REG_MUL * uart_ctx[n].addr_interval)
#define DIV(n)  (uart_ctx[n].base + UART_REG_DIV * uart_ctx[n].addr_interval)

/* RBR: Receive Buffer register     (BLAB bit = 0)  */
#define UART_REG_RBR		(0)
/* THR: Transmit Holding register   (BLAB bit = 0)  */
#define UART_REG_THR		(0)
/* IER: Interrupt Enable register   (BLAB bit = 0)  */
#define UART_REG_IER		(1)

#define FCR_FIFO_SIZE_16	(0x00)
#define FCR_FIFO_SIZE_64	(0x20)
#define FCR_ITL_FIFO_64_BYTES_1	(0x00)

/* FCR: FIFO Control register */
#define UART_REG_FCR		(2)
#define FCR_FIFO_ENABLE		(0x01)
#define FCR_RESET_RX		(0x02)
#define FCR_RESET_TX		(0x04)

/* LCR: Line Control register */
#define UART_REG_LCR		(3)
#define LCR_DLAB                (0x80)
#define LCR_5BIT_CHR            (0x00)
#define LCR_6BIT_CHR            (0x01)
#define LCR_7BIT_CHR            (0x02)
#define LCR_8BIT_CHR            (0x03)
#define LCR_BIT_CHR_MASK        (0x03)
#define LCR_SB                  (0x40)	/*Set Break */

/* MCR: Modem Control register */
#define UART_REG_MCR		(4)
#define MCR_DTR			(0x1)
#define MCR_RTS			(0x2)
#define MCR_LOO			(0x10)
#define MCR_INTR_ENABLE		(0x08)
#define MCR_AUTO_FLOW_EN	(0x20)

/* LSR: Line Status register */
#define UART_REG_LSR		(5)
#define LSR_DR			(0x01)	/* Data Ready */
#define LSR_OE			(0x02)	/* Overrun error */
#define LSR_PE			(0x04)	/* Parity error */
#define LSR_FE			(0x08)	/* Framing error */
#define LSR_BI			(0x10)	/* Breaking interrupt */
#define LSR_THR_EMPTY		(0x20)	/* Non FIFO mode: Transmit holding
					 * register empty
					 */
#define LSR_TDRQ		(0x20)	/* FIFO mode: Transmit Data request */
#define LSR_TEMT		(0x40)	/* Transmitter empty */

/* MSR: Modem Status register */
#define UART_REG_MSR		(6)

/* DLL: Divisor Latch Reg. low byte  (BLAB bit = 1) */
#define UART_REG_DLL		(0)

/* DLH: Divisor Latch Reg. high byte (BLAB bit = 1) */
#define UART_REG_DLH		(1)

/* DLH: Divisor Latch Fractional. (BLAB bit = 1) */
#define UART_REG_DLD		(2)

/* FOR: Fifo O Register (ISH only) */
#define UART_REG_FOR		(0x20)
#define FOR_OCCUPANCY_OFFS      0
#define FOR_OCCUPANCY_MASK      0x7F

/* ABR: Auto-Baud Control Register (ISH only) */
#define UART_REG_ABR		(0x24)
#define ABR_UUE			(0x10)

/* Pre-Scalar Register (ISH only) */
#define UART_REG_PS		(0x30)

/* DDS registers (ISH only) */
#define UART_REG_MUL		(0x34)
#define UART_REG_DIV		(0x38)

/* G_IEN: Global Interrupt Enable  (ISH only) */
#define HSU_REG_GIEN		(0)
#define HSU_REG_GIST		(4)

#define GIEN_PWR_MGMT           (0x01000000)
#define GIEN_DMA_EN             (0x00000020)
#define GIEN_UART2_EN           (0x00000004)
#define GIEN_UART1_EN           (0x00000002)
#define GIEN_UART0_EN           (0x00000001)
#define GIST_DMA_EN             (0x00000020)
#define GIST_UART2_EN           (0x00000004)
#define GIST_UART1_EN           (0x00000002)
#define GIST_UART0_EN           (0x00000001)
#define GIST_UARTx_EN           (GIST_UART0_EN|GIST_UART1_EN|GIST_UART2_EN)

/* UART config flag, send to sc_io_control if the current UART line has HW
 * flow control lines connected.
 */
#define UART_CONFIG_HW_FLOW_CONTROL       (1<<0)

 /* UART config flag for sc_io_control.  If defined a sc_io_event_rx_msg is
  * raised only when the rx buffer is completely full. Otherwise, the event
  * is raised after a timeout is received on the UART line,
  * and all data received until now is provided.
  */
#define UART_CONFIG_DELIVER_FULL_RX_BUF   (1<<1)

/* UART config flag for sc_io_control.  If defined a sc_io_event_rx_buf_depleted
 * is raised when all rx buffers that were added are full. Otherwise, no
 * event is raised.i
 */
#define UART_CONFIG_ANNOUNCE_DEPLETED_BUF (1<<2)

#define UART_GET_CTS_IOCTL		0x1
#define UART_UPDATE_APP_INFO		0x2
#define UART_EVENT_RX_BUFFER_AVAIL	0x1
#define UART_EVENT_RX_BUFFERS_DEPLETED	0x2
#define UART_EVENT_TX_DONE		0x4
#define UART_EVENT_RX_ERROR		0x8
#define UART_EVENT_TX_ERROR		0x10
#define UART_MAX_RX_BUFFERS		20
#define UART_MAX_TX_BUFFERS		20
#define UART_INT_DEVICES		2
#define UART_EXT_DEVICES		8
#define UART_DEVICES			UART_INT_DEVICES
#define UART_ISH_ADDR_INTERVAL		1

#define B9600				0x0000d
#define B57600				0x00000018
#define B115200				0x00000011
#define B921600				0x00000012
#define B2000000			0x00000013
#define B3000000			0x00000014
#define B3250000			0x00000015
#define B3500000			0x00000016
#define B4000000			0x00000017
#define B19200				0x0000e
#define B38400				0x0000f

/* KHZ, MHZ */
#define KHZ(x)				((x) * 1000)
#define MHZ(x)				(KHZ(x) * 1000)
#define UART_ISH_INPUT_FREQ		MHZ(120)
#define UART_BUF_SIZE_MAX		(2048)
#define UART_DEFAULT_BAUD_RATE		115200
#define UART_INT_FIFO_SIZE		16
#define UART_INT_ITL_SIZE		14
#define UART_EXT_FIFO_SIZE		256
#define UART_EXT_ITL_SIZE		250
#define UART_RX_TRIG_SMALL		0
#define UART_RX_TRIG_LARGE		1
#define RX_READY_FLAG			(1 << 0)
#define TX_READY_FLAG			(1 << 1)

#define UART_STATE_READ			(1 << UART_OP_READ)
#define UART_STATE_WRITE		(1 << UART_OP_WRITE)
#define UART_STATE_CG			(1 << UART_OP_CG)

typedef enum {
	UART_PORT_0,
	UART_PORT_1,
	UART_PORT_MAX
} UART_PORT;

typedef struct {
	int32_t handle;
	unsigned int event_flag_rx;
	unsigned int event_flag_tx;
	int io_events_mask;
	void *handle_event_cb;
	uint32_t uart_events;
} uart_info_t;

typedef struct _uart_device_stat {
	uint32_t bytes_in;
	uint32_t bytes_out;
	uint32_t rx_errors;
} uart_device_stat;

typedef enum {
	UART_OP_READ,
	UART_OP_WRITE,
	UART_OP_CG,
	UART_OP_MAX
} UART_OP;

enum {
	BAUD_IDX,
	BAUD_SPEED,
	BAUD_TABLE_MAX
};

typedef struct {
	uint32_t id;
	uint32_t base;
	uint32_t dma_base;
	uint32_t addr_interval;
	uint32_t uart_state;
	uint32_t is_open;
	uart_info_t *uart_info;
	void *rx_bufs[UART_MAX_RX_BUFFERS + 1];
	size_t rx_bufs_size[UART_MAX_RX_BUFFERS + 1];
	void *tx_bufs[UART_MAX_RX_BUFFERS + 1];
	size_t tx_bufs_size[UART_MAX_RX_BUFFERS + 1];
	uint32_t active_rx_buf_offset;
	uint32_t fifo_trigger;
	int baud_rate;
	uint32_t input_freq;
	uint32_t tx_buf_offset;
	uint32_t client_flags;
	uint8_t ier_value;
	uint8_t rx_free_index;
	uint8_t rx_used_index;
	uint8_t rx_last_index;
	uint8_t tx_first_index;
	uint8_t tx_send_index;
	uint8_t tx_last_index;
	uart_device_stat *stat;
} UART_CTX;

#endif	/* _CROS_EC_UART_DEFS_H_ */
