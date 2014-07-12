/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Pericom PI3USB9281 USB charger detection.
 */

#ifndef PI3USB9281_H
#define PI3USB9281_H

#define PI3USB9281_REG_DEV_ID      0x01
#define PI3USB9281_REG_CONTROL     0x02
#define PI3USB9281_REG_INT         0x03
#define PI3USB9281_REG_INT_MASK    0x05
#define PI3USB9281_REG_DEV_TYPE    0x0a
#define PI3USB9281_REG_CHG_STATUS  0x0e
#define PI3USB9281_REG_MANUAL      0x13
#define PI3USB9281_REG_RESET       0x1b
#define PI3USB9281_REG_VBUS        0x1d

#define PI3USB9281_CTRL_SW_OPEN    (1 << 4)
#define PI3USB9281_CTRL_SW_MANUAL  (1 << 2)
#define PI3USB9281_CTRL_INT_MASK   (1 << 0)

#define PI3USB9281_INT_ATTACH      (1 << 0)
#define PI3USB9281_INT_DETACH      (1 << 1)
#define PI3USB9281_INT_OVP         (1 << 5)
#define PI3USB9281_INT_OCP         (1 << 6)
#define PI3USB9281_INT_RECOVERY    (1 << 7)

#define PI3USB9281_TYPE_MHL        (1 << 0)
#define PI3USB9281_TYPE_OTG        (1 << 1)
#define PI3USB9281_TYPE_SDP        (1 << 2)
#define PI3USB9281_TYPE_CAR_KIT    (1 << 4)
#define PI3USB9281_TYPE_CDP        (1 << 5)
#define PI3USB9281_TYPE_DCP        (1 << 6)
#define PI3USB9281_TYPE_APPL_1A    (1 << 10)
#define PI3USB9281_TYPE_APPL_2A    (1 << 11)
#define PI3USB9281_TYPE_APPL_2_4A  (1 << 12)

#define PI3USB9281_MANUAL_VBUS     (3 << 0)
#define PI3USB9281_MANUAL_DP       (1 << 2)
#define PI3USB9281_MANUAL_DM       (1 << 5)

/* Read PI3USB9281 register */
uint8_t pi3usb9281_read(int chip_idx, uint8_t reg);

/* Write PI3USB9281 register */
int pi3usb9281_write(int chip_idx, uint8_t reg, uint8_t val);

/* Enable interrupts */
int pi3usb9281_enable_interrupts(int chip_idx);

/* Disable all interrupts */
int pi3usb9281_disable_interrupts(int chip_idx);

/* Set interrupt mask */
int pi3usb9281_set_interrupt_mask(int chip_idx, uint8_t mask);

/*
 * Get and clear current interrupt status. Return value is a combination of
 * PI3USB9281_INT_*
 */
int pi3usb9281_get_interrupts(int chip_idx);

/* Get but keep interrupt status */
int pi3usb9281_peek_interrupts(int chip_idx);

/*
 * Get attached device type. Return value is a combination of
 * PI3USB9281_TYPE_*
 */
int pi3usb9281_get_device_type(int chip_idx);

/* Return whether VBUS is valid */
int pi3usb9281_get_vbus(int chip_idx);

/* Reset PI3USB9281 */
void pi3usb9281_reset(int chip_idx);

#endif /* PI3USB9281_H */
