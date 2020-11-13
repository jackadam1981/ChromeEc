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

extern const struct tcpm_drv ccgxxf_tcpm_drv;
extern const struct ppc_drv ccgxxf_ppc_drv;

#endif /* __CROS_EC_DRIVER_TCPM_CCGXXF_H */
