/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __CROS_EC_UCPD_STM32GX_H
#define __CROS_EC_UCPD_STM32GX_H

#include "usb_pd_tcpm.h"

#define UCPD_SYNC1 0x18u
#define UCPD_SYNC2 0x11u
#define UCPD_SYNC3 0x06u
#define UCPD_RST1  0x07u
#define UCPD_RST2  0x19u
#define UCPD_EOP   0x0Du

enum ucpd_tx_ordset {
	TX_ORDERSET_SOP =	(UCPD_SYNC1 |
				(UCPD_SYNC1<<5u) |
				(UCPD_SYNC1<<10u) |
				(UCPD_SYNC2<<15u)),
	TX_ORDERSET_SOP1 =	(UCPD_SYNC1 |
				(UCPD_SYNC1<<5u) |
				(UCPD_SYNC3<<10u) |
				(UCPD_SYNC3<<15u)),
	TX_ORDERSET_SOP2 =	(UCPD_SYNC1 |
				(UCPD_SYNC3<<5u) |
				(UCPD_SYNC1<<10u) |
				(UCPD_SYNC3<<15u)),
	TX_ORDERSET_HARD_RESET =	(UCPD_RST1  |
					(UCPD_RST1<<5u) |
					(UCPD_RST1<<10u)  |
					(UCPD_RST2<<15u)),
	TX_ORDERSET_CABLE_RESET =
					(UCPD_RST1 |
					(UCPD_SYNC1<<5u) |
					(UCPD_RST1<<10u)  |
					(UCPD_SYNC3<<15u)),
	TX_ORDERSET_SOP1_DEBUG =	(UCPD_SYNC1 |
					(UCPD_RST2<<5u) |
					(UCPD_RST2<<10u) |
					(UCPD_SYNC3<<15u)),
	TX_ORDERSET_SOP2_DEBUG =	(UCPD_SYNC1 |
					(UCPD_RST2<<5u) |
					(UCPD_SYNC3<<10u) |
					(UCPD_SYNC2<<15u)),
};

/* STM32 UCPD driver for Chrome EC */
int stm32gx_ucpd_init(int port);
int stm32gx_ucpd_release(int port);
int stm32gx_ucpd_get_cc(int port, enum tcpc_cc_voltage_status *cc1,
			enum tcpc_cc_voltage_status *cc2);
int stm32gx_ucpd_get_role_control(int port);
int stm32gx_ucpd_set_cc(int port, int cc_pull, int rp);
int stm32gx_ucpd_set_polarity(int port, enum tcpc_cc_polarity polarity);
int stm32gx_ucpd_set_rx_enable(int port, int enable);
int stm32gx_ucpd_set_msg_header(int port, int power_role, int data_role);
int stm32gx_ucpd_transmit(int port,
			enum tcpm_transmit_type type,
			uint16_t header,
			  const uint32_t *data);
int stm32gx_ucpd_get_message_raw(int port, uint32_t *payload, int *head);

#endif /* __CROS_EC_UCPD_STM32GX_H */
