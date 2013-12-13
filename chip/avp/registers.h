/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Register map for AVP processor
 */

#ifndef __CROS_EC_REGISTERS_H
#define __CROS_EC_REGISTERS_H

#include "common.h"

/* --- IRQs --- */

#define AVP_IRQ_UARTA           36

/* --- UART --- */
#define AVP_UARTA_BASE          0x70006000
#define AVP_UARTB_BASE          0x70006040
#define AVP_UARTC_BASE          0x70006200
#define AVP_UARTD_BASE          0x70006300
#define AVP_UARTE_BASE          0x70006400

#define AVP_UART_BASE(n)        CONCAT3(AVP_UART, n, _BASE)
#define AVP_UART_REG(n, offset) REG8(AVP_UART_BASE(n) + (offset))

#define AVP_UART_DLL(n)         AVP_UART_REG(n, 0x00)
#define AVP_UART_DLM(n)         AVP_UART_REG(n, 0x04)
#define AVP_UART_RBR(n)         AVP_UART_REG(n, 0x00)
#define AVP_UART_THR(n)         AVP_UART_REG(n, 0x00)
#define AVP_UART_IER(n)         AVP_UART_REG(n, 0x04)
#define AVP_UART_IIR(n)         AVP_UART_REG(n, 0x08)
#define AVP_UART_FCR(n)         AVP_UART_REG(n, 0x08)
#define AVP_UART_LCR(n)         AVP_UART_REG(n, 0x0C)
#define AVP_UART_MCR(n)         AVP_UART_REG(n, 0x10)
#define AVP_UART_LSR(n)         AVP_UART_REG(n, 0x14)
#define AVP_UART_MSR(n)         AVP_UART_REG(n, 0x18)
#define AVP_UART_SCR(n)         AVP_UART_REG(n, 0x1C)
#define AVP_UART_RX_FIFO_CFG(n) AVP_UART_REG(n, 0x24)
#define AVP_UART_MIE(n)         AVP_UART_REG(n, 0x28)
#define AVP_UART_ASR(n)         AVP_UART_REG(n, 0x3c)

/* --- Clock and Reset --- */
#define AVP_CAR_BASE            0x60006000

#define AVP_CAR_REG(offset)     REG32(AVP_CAR_BASE + (offset))

#define AVP_CAR_RST_SRC         AVP_CAR_REG(0x00)
#define AVP_CAR_RST_DEV_L       AVP_CAR_REG(0x04)
#define AVP_CAR_RST_DEV_H       AVP_CAR_REG(0x08)
#define AVP_CAR_RST_DEV_U       AVP_CAR_REG(0x0C)
#define AVP_CAR_OUT_ENB_L       AVP_CAR_REG(0x10)
#define AVP_CAR_OUT_ENB_H       AVP_CAR_REG(0x14)
#define AVP_CAR_OUT_ENB_U       AVP_CAR_REG(0x18)

#define AVP_CAR_CLK_SRC_UARTA   AVP_CAR_REG(0x178)

#define CAR_OFFSET_UARTA        (1 << 6)

#define CLK_SOURCE_SHIFT        29
#define CLK_UART_DIV_OVERRIDE   (1 << 24)
#define TEGRA_CLK_M_KHZ         12000
#define CLK_DIVIDER(REF, FREQ)  ((((REF) * 2) / FREQ) - 2)

enum clock_source {
        PLLP = 0,
        PLLC2 = 1,
        PLLC = 2,
        PLLD = 2,
        PLLC3 = 3,
        PLLA = 3,
        PLLM = 4,
        PLLD2 = 5,
        CLK_M = 6,
};

/* --- GPIO --- */

#define DUMMY_GPIO_BANK         0

#endif /* __CROS_EC_REGISTERS_H */
