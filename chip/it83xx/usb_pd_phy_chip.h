/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USB_PD_PHY_CHIP_H
#define __CROS_EC_USB_PD_PHY_CHIP_H

extern void ite8320_power_enable_vbus(uint8_t u8Port, uint8_t bIsEnable);
extern uint32_t ite8320_power_get_m_volt(uint8_t u8Port);
#endif /* __CROS_EC_USB_PD_PHY_CHIP_H */
