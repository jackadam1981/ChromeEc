/*
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This adds the extra pin assignment needed for fingerprint capabilities.
 *
 * WARNING: The pin assignment is not identical to bloonchipper!
 *          This is because the Nucleo builtin LEDs/Button overlap pins.
 */

#include "base-gpio.inc"

/* Interrupts */
GPIO_INT(PCH_SLP_S0_L,    PIN(A, 8),  GPIO_INT_BOTH, slp_event)
GPIO_INT(PCH_SLP_S3_L,    PIN(B, 6),  GPIO_INT_BOTH, slp_event)

/* Output for User Presence */
GPIO(USER_PRES_L,         PIN(B, 9),  GPIO_ODR_HIGH)

/* Fingerprint Sensor */
GPIO(DIVIDER_HIGHSIDE,    PIN(C, 8),  GPIO_OUT_LOW)
GPIO(FP_SENSOR_SEL,       PIN(C, 0),  GPIO_INPUT)
GPIO(TRANSPORT_SEL,       PIN(C, 1),  GPIO_INPUT)
GPIO_INT(FPS_INT,         PIN(A, 0),  GPIO_INT_RISING, fps_event)
GPIO(FP_RST_ODL,          PIN(B, 10), GPIO_OUT_HIGH)
GPIO(SPI2_NSS,            PIN(B, 12), GPIO_OUT_HIGH)
/*
 * SPI2 master to sensor: PB13/14/15 (CLK/MISO/MOSI)
 * Note that we're not configuring NSS (PB12) here because we have already
 * configured it as a GPIO above and the SPI_MASTER module expects to use it
 * in software NSS management mode, not hardware management mode.
 */
ALTERNATE(PIN_MASK(B, 0xE000), GPIO_ALT_SPI, MODULE_SPI_MASTER,  0)

/*
 * Unused pins.
 * Configuring unused pins as ANALOG INPUT to save power. For more info
 * look at "USING STM32F4 MCU POWER MODES WITH BEST DYNAMIC EFFICIENCY"
 * ("AN4365") section 1.2.6 and STM32F412 reference manual section 7.3.12.
 */
UNUSED(PIN(A, 2))
UNUSED(PIN(A, 3))
UNUSED(PIN(B, 2))
UNUSED(PIN(B, 5))
UNUSED(PIN(C, 14))
UNUSED(PIN(C, 15))
UNUSED(PIN(H, 0))
UNUSED(PIN(H, 1))
