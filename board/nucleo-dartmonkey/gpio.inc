/*
 * Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Note that these pins map to the Nucleo-H743ZI V2 and are only slightly
 * compatible with the original version.
 *
 * The V2 is denoted by "Nucleo-H743ZI2" vs. "Nucleo-H743ZI".
 */

#include "base-gpio.inc"

/* Interrupts */
GPIO_INT(SLP_L,           PIN(D, 13), GPIO_INT_BOTH, slp_event)
GPIO_INT(SLP_ALT_L,       PIN(A, 11), GPIO_INT_BOTH, slp_event)

/* Output for User Presence */
GPIO(USER_PRES_L,         PIN(C, 5),  GPIO_ODR_HIGH)

/* Fingerprint Sensor */
GPIO_INT(FPS_INT,         PIN(A, 0),  GPIO_INT_RISING, fps_event)
GPIO(FP_RST_ODL,          PIN(E, 0),  GPIO_OUT_HIGH)
GPIO(SPI4_NSS,            PIN(E, 4),  GPIO_OUT_HIGH)
/* SPI4 master to sensor: PE2/5/6 (CLK/MISO/MOSI) */
ALTERNATE(PIN_MASK(E, 0x0064), GPIO_ALT_SPI, MODULE_SPI_CONTROLLER,  0)

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
