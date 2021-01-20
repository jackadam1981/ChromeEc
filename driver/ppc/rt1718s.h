/* Copyright 2018 The Richtek Technology Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*Richtek RT1718S Type-C Power Path Controller */

#ifndef __CROS_EC_RT1718S_H
#define __CROS_EC_RT1718S_H

/* I2C addresses */
#define RT1718S_ADDR0_FLAGS							(0x43)

#define RT1718S_GPIO1_CTRL							(0xED)
#define RT1718S_GPIO2_CTRL							(0xEE)

#define RT1718S_RT2_VBUS_OCRC_EN					(0xF214)
#define RT1718S_RT2_VBUS_OCRC_EN_VBUS_OCP1_EN		(0x01)

struct ppc_drv;
extern const struct ppc_drv rt1718s_ppc_drv;

void rt1718s_interrupt(int port);

#endif /* defined(__CROS_EC_RT1718S_H) */

