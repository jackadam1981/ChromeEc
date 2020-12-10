/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Richtek RT1715 Type-C port controller */
#ifndef __CROS_EC_USB_PD_TCPM_RT1715_H
#define __CROS_EC_USB_PD_TCPM_RT1715_H
/* I2C interface */
#define RT1715_I2C_ADDR_FLAGS 0x4E
#define RT1715_VENDOR_ID 0x29CF
#define RT1715_REG_VENDOR_5 0x9B
#define RT1715_REG_VENDOR_5_SHUTDOWN_OFF BIT(5)
#define RT1715_REG_VENDOR_5_ENEXTMSG BIT(4)
#define RT1715_REG_VENDOR_7 0xA0
#define RT1715_REG_VENDOR_7_SOFT_RESET BIT(0)
#define RT1715_REG_PHY_CTRL1 0x80
#define RT1715_REG_PHY_CTRL2 0x81
#define RT1715_REG_BMCIO_RXDZSEL 0x93
#define RT1715_MASK_OCCTRL_SEL 0xE0
#define RT1715_OCCTRL_600MA 0x80
#define RT1715_MASK_BMCIO_RXDZSEL BIT(0)
#define RT1715_REG_I2CRST_CTRL 0x9E
#define RT1715_REG_TTCPC_FILTER 0xA1
#define RT1715_REG_DRP_TOGGLE_CYCLE 0xA2
#define RT1715_REG_DRP_DUTY_CTRL 0xA3
#define RT1715_REG_BMCIO_RXDZEN 0xAF

/* timeout = (tout+1) * 12.5ms */
#define RT1715_REG_I2CRST_SET(en, tout) ((en << 7) | (tout & 0x0f))
extern const struct tcpm_drv rt1715_tcpm_drv;
#endif /* defined(__CROS_EC_USB_PD_TCPM_RT1715_H) */
