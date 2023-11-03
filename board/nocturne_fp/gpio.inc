/*
 * Copyright 2017 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Interrupts */
GPIO_INT(SLP_L,           PIN(D,13), GPIO_INT_BOTH, slp_event)
GPIO_INT(SLP_ALT_L,       PIN(A,11), GPIO_INT_BOTH, slp_event)
GPIO_INT(SPI1_NSS,        PIN(A, 4), GPIO_INPUT, spi_event)

/* Inputs */
#ifndef APPLY_RESET_LOOP_FIX
GPIO(WP,                  PIN(B, 7), GPIO_INPUT)
#endif

/* Outputs */
GPIO(EC_INT_L,            PIN(A, 1),  GPIO_OUT_HIGH)

UNIMPLEMENTED(ENTERING_RW)

/* USART1: PA9/PA10 (TX/RX) */
ALTERNATE(PIN_MASK(A, 0x0600), GPIO_ALT_USART,  MODULE_UART, GPIO_PULL_UP)
/* SPI1 slave from the AP: PA4/5/6/7 (CS/CLK/MISO/MOSI) */
ALTERNATE(PIN_MASK(A, 0x00f0), GPIO_ALT_SPI, MODULE_SPI,  0)

/*
 * Debug Port Default Configuration.
 *
 * We need to reference the port/pins for the debug interface here to ensure
 * that the gpio clocks are enabled for the port. Otherwise, our attempts
 * to write to the gpio registers will be simple be ignored.
 *
 * These pins and pulls are listed in the STM32F412 Reference Manual
 * (RM0402 Rev 5) section 7.3.1, as the following:
 *  PA15: JTDI in pull-up
 *  PA14: JTCK/SWCLK in pull-down
 *  PA13: JTMS/SWDAT in pull-up
 *  PB4:  NJTRST in pull-up
 *  PB3:  JTDO in floating state
 */
ALTERNATE(PIN_MASK(A, BIT(15)|BIT(13)), GPIO_ALT_SYS,  MODULE_DEBUG,
    GPIO_PULL_UP)
ALTERNATE(PIN_MASK(A, BIT(14)), GPIO_ALT_SYS,  MODULE_DEBUG, GPIO_PULL_DOWN)
ALTERNATE(PIN_MASK(B, BIT(4)),  GPIO_ALT_SYS,  MODULE_DEBUG, GPIO_PULL_UP)
ALTERNATE(PIN_MASK(B, BIT(3)),  GPIO_ALT_SYS,  MODULE_DEBUG, 0)

#ifdef SECTION_IS_RW
#include "gpio_rw.inc"
#endif
