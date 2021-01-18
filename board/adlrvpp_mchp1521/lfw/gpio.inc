/* -*- mode:c -*-
 *
 * Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Minimal set of GPIOs needed for LFW loader
 */

/*
 * MEC15211H GPIO_0055/PWM2/SHD_CS0#/RSMRST#
 * MEC1521H QMSPI controller drives chip select, must be
 * configured to alternative function.
 */
GPIO(EC_SPI_CS0_N,	PIN(055),	GPIO_ODR_HIGH)
/* Alternate functions */

/* SPI pins */
/*
 * MEC1521H SHD SPI is connected to QMSPI controller.
 * QMSPI drives chip select.
 * EC_SPI_CS0_N  = GPIO_0055/SHD_CS0_N(Bank 1, bit[13]) Func2
 * EC_SPI_CS1_N  = GPIO_0002/SHD_CS1_N(Bank 0, bit[2]) Func2
 * EC_SPI_CLK  = GPIO_0056/SHD_CLK(Bank 1, bit[14]) Func2
 * EC_SPI_MOSI  = GPIO_0223/SHD_IO0(Bank 4, bit[19]) Func1
 * EC_SPI_MISO  = GPIO_0224/SHD_IO1(Bank 4, bit[20]) Func2
 * EC_SPI_WP  = GPIO_0227/SHD_IO2(Bank 4, bit[23]) Func1
 * EC_SPI_HOLD  = GPIO_0016/SHD_IO3(Bank 0, bit[14]) Func2
 */
ALTERNATE(PIN_MASK(1, 0x2000),	2,	MODULE_SPI_FLASH,	GPIO_ODR_HIGH)
ALTERNATE(PIN_MASK(1, 0x4000),	2,	MODULE_SPI_FLASH,	0)
ALTERNATE(PIN_MASK(4, 0x00080000),	1,	MODULE_SPI_FLASH,	0)
ALTERNATE(PIN_MASK(4, 0x00100000),	2,	MODULE_SPI_FLASH,	0)
