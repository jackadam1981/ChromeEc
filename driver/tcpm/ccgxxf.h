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

/* CCGXXF Firmware version location in binary image */
#define CCGXXF_BUILD_NUM_OFFSET		0xE0

#define CCGXXF_BUILD_VER_OFFSET		0xE2
#define CCGXXF_BUILD_VER_MINOR_OFFSET	0xE2
#define CCGXXF_BUILD_VER_MAJOR_OFFSET	0xE3

/* CCGXXF vendor info */
#define CCGXXF_VENDOR_ID		0x04B4
#define CCGXXF_PRODUCT_ID_CCG6DF	0xF6EE
#define CCGXXF_PRODUCT_ID_CCG6SF	0xF6EF

/* CCGXXF register definitions */
#define CCGXXF_REG_FWU_RESPONSE		0x90
#define CCGXXF_FWU_RES_RETRY		5
#define CCGXXF_FWU_RES_SUCCESS		0x0000
#define CCGXXF_FWU_RES_CMD_FAILED	0x0003
#define CCGXXF_FWU_RES_CMD_IN_PROGRESS	0x00FF
#define CCGXXF_FWU_RES_FW_REGION_1	0x0100
#define CCGXXF_FWU_RES_FW_REGION_2	0x0200

#define CCGXXF_REG_FWU_COMMAND		0x92
#define CCGXXF_FWU_CMD_ENABLE		0x0011
#define CCGXXF_FWU_CMD_DISABLE		0x0022
#define CCGXXF_FWU_CMD_GET_FW_MODE	0x0033
#define CCGXXF_FWU_CMD_WRITE_FLASH_ROW	0x0044
#define CCGXXF_FWU_CMD_VALIDATE_FW_IMG	0x0066
#define CCGXXF_FWU_CMD_RESET		0x0077

#define CCGXXF_REG_FW_VERSION		0x94
#define CCGXXF_REG_FW_VERSION_BUILD	0x96

#define CCGXXF_REG_FWU_BUFFER		0x98
#define CCGXXF_FWU_BUFFER_ROW_SIZE	128
#define CCGXXF_FWU_BUFFER_ROWS		(189 + 1)

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
	CCGXXF_IO_0,
	CCGXXF_IO_1,
	CCGXXF_IO_2,
	CCGXXF_IO_3,
	CCGXXF_IO_4,
	CCGXXF_IO_5,
	CCGXXF_IO_6,
	CCGXXF_IO_7
};

#define CCGXXF_REG_GPIO_CONTROL(port)	((port) + 0x80)
#define CCGXXF_REG_GPIO_STATUS(port)	((port) + 0x84)

#define CCGXXF_REG_GPIO_MODE		0x88
#define CCGXXF_GPIO_PIN_MASK_SHIFT	8
#define CCGXXF_GPIO_PIN_MODE_SHIFT	2
#define CCGXXF_GPIO_1P8V_SEL		BIT(7)

enum ccgxxf_gpio_mode {
	CCGXXF_GPIO_MODE_HIZ_ANALOG,
	CCGXXF_GPIO_MODE_HIZ_DIGITAL,
	CCGXXF_GPIO_MODE_RES_UP,
	CCGXXF_GPIO_MODE_RES_DWN,
	CCGXXF_GPIO_MODE_OD_LOW,
	CCGXXF_GPIO_MODE_OD_HIGH,
	CCGXXF_GPIO_MODE_STRONG,
	CCGXXF_GPIO_MODE_RES_UPDOWN
};

extern const struct ioexpander_drv ccgxxf_ioexpander_drv;

#endif /* CONFIG_IO_EXPANDER_CCGXXF */

#endif /* __CROS_EC_DRIVER_TCPM_CCGXXF_H */
