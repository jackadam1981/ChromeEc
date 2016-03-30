/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power delivery port management - common header for TCPM drivers */

#ifndef __CROS_EC_USB_PD_TCPM_TCPM_H
#define __CROS_EC_USB_PD_TCPM_TCPM_H

#include "gpio.h"
#include "i2c.h"
#include "usb_pd_tcpm.h"

extern const struct tcpc_config_t tcpc_config[];

#ifndef CONFIG_USB_PD_TCPC
/* I2C wrapper functions - get I2C port / slave addr from config struct. */
static inline int tcpc_write(int port, int reg, int val)
{
	return i2c_write8(tcpc_config[port].i2c_host_port,
			  tcpc_config[port].i2c_slave_addr,
			  reg, val);
}

static inline int tcpc_write16(int port, int reg, int val)
{
	return i2c_write16(tcpc_config[port].i2c_host_port,
			   tcpc_config[port].i2c_slave_addr,
			   reg, val);
}

static inline int tcpc_read(int port, int reg, int *val)
{
	return i2c_read8(tcpc_config[port].i2c_host_port,
			 tcpc_config[port].i2c_slave_addr,
			 reg, val);
}

static inline int tcpc_read16(int port, int reg, int *val)
{
	return i2c_read16(tcpc_config[port].i2c_host_port,
			  tcpc_config[port].i2c_slave_addr,
			  reg, val);
}

static inline int tcpc_xfer(int port,
			    const uint8_t *out, int out_size,
			    uint8_t *in, int in_size,
			    int flags)
{
	return i2c_xfer(tcpc_config[port].i2c_host_port,
			tcpc_config[port].i2c_slave_addr,
			out, out_size,
			in, in_size,
			flags);
}

static inline void tcpc_lock(int port, int lock)
{
	i2c_lock(tcpc_config[port].i2c_host_port, lock);
}
#endif

/* TCPM driver wrapper function */
static inline int tcpm_init(int port) {
	return tcpc_config[port].drv->init(port);
}

static inline int tcpm_alert_status(int port, int *alert) {
	return tcpc_config[port].drv->alert_status(port, alert);
}

static inline int tcpm_alert_mask_set(int port, uint16_t mask) {
	return tcpc_config[port].drv->alert_mask_set(port, mask);
}

static inline int tcpm_get_cc(int port, int *cc1, int *cc2) {
	return tcpc_config[port].drv->get_cc(port, cc1, cc2);
}

static inline int tcpm_get_vbus_level(int port) {
	return tcpc_config[port].drv->get_vbus_level(port);
}

static inline int tcpm_set_cc(int port, int pull) {
	return tcpc_config[port].drv->set_cc(port, pull);
}

static inline int tcpm_set_polarity(int port, int polarity) {
	return tcpc_config[port].drv->set_polarity(port, polarity);
}

static inline int tcpm_set_power_status_mask(int port, uint8_t mask) {
	return tcpc_config[port].drv->set_power_status_mask(port, mask);
}

static inline int tcpm_set_vconn(int port, int enable) {
	return tcpc_config[port].drv->set_vconn(port, enable);
}

static inline int tcpm_set_msg_header(int port, int power_role, int data_role) {
	return tcpc_config[port].drv->set_msg_header(port, power_role, data_role);
}

static inline int tcpm_set_rx_enable(int port, int enable) {
	return tcpc_config[port].drv->set_rx_enable(port, enable);
}

static inline int tcpm_get_message(int port, uint32_t *payload, int *head) {
	return tcpc_config[port].drv->get_message(port, payload, head);
}

static inline int tcpm_transmit(int port, enum tcpm_transmit_type type, uint16_t header,
		   const uint32_t *data) {
	return tcpc_config[port].drv->transmit(port, type, header, data);
}

#endif
