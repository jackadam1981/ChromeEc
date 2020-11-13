/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power delivery port management */
/* For Cypress EZ-PD CCG6DF, CCG6SF */
#ifndef __CROS_EC_DRIVER_TCPM_CCGXXF_H
#define __CROS_EC_DRIVER_TCPM_CCGXXF_H

extern const struct tcpm_drv ccgxxf_tcpm_drv;
extern const struct ppc_drv ccgxxf_ppc_drv;

/* list all the pins that can be configured as GPIOs */
enum ccgxxf_gpios {
	UART_TX_P1,
	UART_RX_P1,
	I2C_SDA_SCB2,
};

/* Set the GPIO */
int ccgxxf_gpio_set(int port, enum ccgxxf_gpios gpio, int val);

/* Get the GPIO */
int ccgxxf_gpio_get(int port, enum ccgxxf_gpios gpio);

#endif /* __CROS_EC_DRIVER_TCPM_CCGXXF_H */
