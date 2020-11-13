/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power delivery port management */
/* For Cypress EZ-PD CCG6DF, CCG6SF */
#ifndef __CROS_EC_DRIVER_TCPM_CCGXXF_H
#define __CROS_EC_DRIVER_TCPM_CCGXXF_H

#define CCGXXF_I2C_ADDR1_FLAGS	0x0B  /* CY_UPD was 0x08 changed to be the same as Chromebook i2c addr. */
#define CCGXXF_I2C_ADDR2_FLAGS	0x40
#define CCGXXF_I2C_ADDR3_FLAGS	0x42

#define TCPC_REG_VENDOR_GPIO_CTRL        (0x80)

extern const struct tcpm_drv ccgxxf_tcpm_drv;
extern const struct ppc_drv ccgxxf_ppc_drv;

/* list all the pins that can be configured as GPIOs */
enum ccgxxf_gpios {
	CCG6_GPIO1 = 1u,
	CCG6_GPIO2 = 2u
};

enum ccgxxf_gpios_state {
	CCG6_GPIO_CLR,
	CCG6_GPIO_SET
};

/* Set the GPIO */
int ccgxxf_gpio_set(int port, enum ccgxxf_gpios gpio, enum ccgxxf_gpios_state val);

/* Get the GPIO */
int ccgxxf_gpio_get(int port, enum ccgxxf_gpios gpio, enum ccgxxf_gpios_state *gpio_value);

#endif /* __CROS_EC_DRIVER_TCPM_CCGXXF_H */
