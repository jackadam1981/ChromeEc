/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power delivery port management */
/* For Cypress EZ-PD CCG6DF, CCG6SF */
#ifndef __CROS_EC_DRIVER_TCPM_CCGXXF_H
#define __CROS_EC_DRIVER_TCPM_CCGXXF_H

#define CCGXXF_I2C_ADDR1_FLAGS	0x0B
#define CCGXXF_I2C_ADDR2_FLAGS	0x40
#define CCGXXF_I2C_ADDR3_FLAGS	0x42

/* CCGXXF built in I/O expander definitions */
#ifdef CONFIG_IO_EXPANDER_CCGXXF

#define CCGXXF_IOEXP_MAX_PORT_CNT			(4u)
#define CCGXXF_IOEXP_VALID_GPIO_MASK		(0xFFu)
#define CCGXXF_IOEXP_PIN_MODE_OFFSET		(2u)
#define CCGXXF_IOEXP_PORT_MASK_OFFSET		(8u)

#define TCPC_REG_CCGXXF_GPIO_P0_DATA_OUT	(0x80u)
#define TCPC_REG_CCGXXF_GPIO_P1_DATA_OUT    (0x81u)
#define TCPC_REG_CCGXXF_GPIO_P2_DATA_OUT    (0x82u)
#define TCPC_REG_CCGXXF_GPIO_P3_DATA_OUT    (0x83u)

#define TCPC_REG_CCGXXF_GPIO_P0_DATA_IN     (0x84u)
#define TCPC_REG_CCGXXF_GPIO_P1_DATA_IN     (0x85u)
#define TCPC_REG_CCGXXF_GPIO_P2_DATA_IN     (0x86u)
#define TCPC_REG_CCGXXF_GPIO_P3_DATA_IN     (0x87u)

#define TCPC_REG_CCGXXF_GPIO_CONFIG_REG     (0x88u)

extern const struct ioexpander_drv ccgxxf_ioexpander_drv;

#endif /* CONFIG_IO_EXPANDER_CCGXXF */

#endif /* __CROS_EC_DRIVER_TCPM_CCGXXF_H */
