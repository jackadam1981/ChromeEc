/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power delivery port management */
/* For ITE83xx */
#ifndef __CROS_EC_DRIVER_TCPM_IT83XX_H
#define __CROS_EC_DRIVER_TCPM_IT83XX_H

extern int tcpc_alert_status(int port, int *alert);
extern int tcpc_alert_status_clear(int port, uint16_t mask);
extern int tcpc_alert_mask_set(int port, uint16_t mask);
extern int tcpc_get_cc(int port, int *cc1, int *cc2);
extern int tcpc_set_cc(int port, int pull);
extern int tcpc_set_polarity(int port, int polarity);
extern int tcpc_set_power_status_mask(int port, uint8_t mask);
extern int tcpc_set_vconn(int port, int enable);
extern int tcpc_set_msg_header(int port, int power_role, int data_role);
extern int tcpc_set_rx_enable(int port, int enable);

extern int tcpc_get_message(int port, uint32_t *payload, int *head);
extern int tcpc_transmit(int port, enum tcpm_transmit_type type,
			 uint16_t header, const uint32_t *data);

#endif /* __CROS_EC_DRIVER_TCPM_IT83XX_H */

