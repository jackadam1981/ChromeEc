/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power delivery port management */

#ifndef __CROS_EC_USB_PD_TCPM_ANX7447_H
#define __CROS_EC_USB_PD_TCPM_ANX7447_H

#define ANX7447_USBC_ADDR 0x52
#define ANX7447_SPI_ADDR 0x62

#define ANX7447_REG_HPD_CTRL_0 0x7E
#define ANX7447_REG_HPD_MODE 0x01
#define ANX7447_REG_HPD_OUT 0x02

#define ANX7447_REG_HPD_DEGLITCH_H 0x80
#define ANX7447_REG_HPD_OEN 0x40

#define ANX7447_REG_TCPC_SWITCH_0 0xB4
#define ANX7447_REG_TCPC_SWITCH_1 0xB5
#define ANX7447_REG_TCPC_AUX_SWITCH 0xB6

int anx7447_update_hpd(int port, int level, int irq);
int anx7447_set_dp_pin_mode(int port, int pin_mode);
int anx7447_enable_cable_detection(int port);
int anx7447_set_power_supply_ready(int port);
int anx7447_power_supply_reset(int port);
int anx7447_hpd_disable(int port);
int anx7447_board_charging_enable(int port, int enable);

void anx7447_hpd_mode_en(void);
void anx7447_hpd_output_en(void);

extern const struct tcpm_drv anx7447_tcpm_drv;
extern const struct usb_mux_driver anx7447_usb_mux_driver;
void anx7447_tcpc_update_hpd_status(int port, int hpd_lvl, int hpd_irq);
void anx7447_tcpc_clear_hpd_status(int port);

#endif /* __CROS_EC_USB_PD_TCPM_ANX7688_H */
