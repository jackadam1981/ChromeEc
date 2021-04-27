/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * USB Power delivery port management For Cypress EZ-PD CCG6DF, CCG6SF
 * CCGXXF FW is designed to adapt standard TCPM driver procedures.
 */
#ifndef __CROS_EC_DRIVER_TCPM_CCGXXF_H
#define __CROS_EC_DRIVER_TCPM_CCGXXF_H

#define CCGXXF_I2C_ADDR1_FLAGS	0x0B
#define CCGXXF_I2C_ADDR2_FLAGS	0x40
#define CCGXXF_I2C_ADDR3_FLAGS	0x42

/* CCGXXF built in I/O expander definitions */
#ifdef CONFIG_IO_EXPANDER_CCGXXF

/* CCGXXF I/O ports that can be referenced in gpio.inc */
enum ccgxxf_io_ports {
	CCGXXF_PORT_0,
	CCGXXF_PORT_1,
	CCGXXF_PORT_2,
	CCGXXF_PORT_3
};

/* CCGXXF I/O pins that can be referenced in gpio.inc */
enum ccgxxf_io_pins {
	CCGXXF_IO_P0,
	CCGXXF_IO_P1,
	CCGXXF_IO_P2,
	CCGXXF_IO_P3,
	CCGXXF_IO_P4,
	CCGXXF_IO_P5,
	CCGXXF_IO_P6,
	CCGXXF_IO_P7
};

/* GPIO_MODE is write only register allows to set pin mode */
#define CCGXXF_REG_GPIO_MODE		0x80
#define CCGXXF_GPIO_PORT_NUM_SHIFT	0x4
/* TODO: Add modes */

/*
 * GPIO_CONTROL is write only register that allows to set, clear or
 * read a single pin defined by port and pin number value.
 */
#define CCGXXF_REG_GPIO_CONTROL		0x82
#define CCGXXF_GPIO_CTRL_SET_LOW	0x800
#define CCGXXF_GPIO_CTRL_SET_HIGH	0x900
#define CCGXXF_GPIO_CTRL_READ_PIN	0xA00

/*
 * GPIO_RESPONSE is read only register that contain response to pin read
 * command issued using GPIO_CONTROL register.
 */
#define CCGXXF_REG_GPIO_RESPONSE	0x84
#define CCGXXF_GPIO_RESP_PIN_STATE_MASK	0x1

extern const struct ioexpander_drv ccgxxf_ioexpander_drv;

#endif /* CONFIG_IO_EXPANDER_CCGXXF */

#endif /* __CROS_EC_DRIVER_TCPM_CCGXXF_H */
