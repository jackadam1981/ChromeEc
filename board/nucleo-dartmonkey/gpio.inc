/*
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
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
GPIO_INT_RW(SLP_L,           PIN(D, 13), GPIO_INT_BOTH, slp_event)
GPIO_INT_RW(SLP_ALT_L,       PIN(A, 11), GPIO_INT_BOTH, slp_event)

/* Output for User Presence */
GPIO_RW(USER_PRES_L,         PIN(C, 5),  GPIO_ODR_HIGH)

/* Fingerprint Sensor */
GPIO_INT_RW(FPS_INT,         PIN(A, 0),  GPIO_INT_RISING, fps_event)
GPIO_RW(FP_RST_ODL,          PIN(E, 0),  GPIO_OUT_HIGH)
GPIO_RW(SPI4_NSS,            PIN(E, 4),  GPIO_OUT_HIGH)
/* SPI4 master to sensor: PE2/5/6 (CLK/MISO/MOSI) */
ALTERNATE_RW(PIN_MASK(E, 0x0064), GPIO_ALT_SPI, MODULE_SPI_MASTER,  0)
