/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USB_PD_PHY_CHIP_H
#define __CROS_EC_USB_PD_PHY_CHIP_H

#include "usb_pd_tcpm.h"

#define SET_MASK(reg, bit_mask)      ((reg) |= (bit_mask))
#define CLEAR_MASK(reg, bit_mask)    ((reg) &= (~(bit_mask)))
#define IS_MASK_SET(reg, bit_mask)   (((reg) & (bit_mask)) != 0)
#define IS_MASK_CLEAR(reg, bit_mask) (((reg) & (bit_mask)) == 0)

/* macros for set */
#define USBPD_KICK_TX_START(port)            \
	SET_MASK(IT83XX_USBPD_MTCR(port),    \
		USBPD_REG_MASK_TX_START)
#define USBPD_SEND_HARD_RESET(port)          \
	SET_MASK(IT83XX_USBPD_MTSR0(port),   \
		USBPD_REG_MASK_SEND_HW_RESET)
#define USBPD_SW_RESET(port)                 \
	SET_MASK(IT83XX_USBPD_GCR(port),     \
		USBPD_REG_MASK_SW_RESET_BIT)
#define USBPD_ENABLE_BMC_PHY(port)           \
	SET_MASK(IT83XX_USBPD_GCR(port),     \
		USBPD_REG_MASK_BMC_PHY)
#define USBPD_DISABLE_BMC_PHY(port)          \
	CLEAR_MASK(IT83XX_USBPD_GCR(port),   \
		USBPD_REG_MASK_BMC_PHY)
#define USBPD_START(port)                    \
	CLEAR_MASK(IT83XX_USBPD_CCGCR(port), \
		USBPD_REG_MASK_DISABLE_CC)
#define USBPD_ENABLE_SEND_BIST_MODE_2(port)  \
	SET_MASK(IT83XX_USBPD_MTSR0(port),   \
		USBPD_REG_MASK_SEND_BIST_MODE_2)
#define USBPD_DISABLE_SEND_BIST_MODE_2(port) \
	CLEAR_MASK(IT83XX_USBPD_MTSR0(port), \
		USBPD_REG_MASK_SEND_BIST_MODE_2)

/* macros for get */
#define USBPD_GET_POWER_ROLE(port)                  \
	(IT83XX_USBPD_PDMSR(port) & 1)
#define USBPD_GET_CC1_PULL_REGISTER_SELECTION(port) \
	(IT83XX_USBPD_CCGCR(port) & (1 << 1))
#define USBPD_GET_CC2_PULL_REGISTER_SELECTION(port) \
	(IT83XX_USBPD_BMCSR(port) & (1 << 3))
#define USBPD_GET_PULL_CC_SELECTION(port)           \
	(IT83XX_USBPD_CCGCR(port) & 1)

/* macros for check */
#define USBPD_IS_TX_ERR(port)       \
	IS_MASK_SET(IT83XX_USBPD_MTCR(port), USBPD_REG_MASK_TX_ERR_STAT)
#define USBPD_IS_TX_DISCARD(port)   \
	IS_MASK_SET(IT83XX_USBPD_MTCR(port), USBPD_REG_MASK_TX_DISCARD_STAT)

/* macros for PD ISR */
#define USBPD_IS_HARD_RESET_DETECT(port) \
	IS_MASK_SET(IT83XX_USBPD_ISR(port), USBPD_REG_MASK_HARD_RESET_DETECT)
#define USBPD_IS_TX_DONE(port)           \
	IS_MASK_SET(IT83XX_USBPD_ISR(port), USBPD_REG_MASK_MSG_TX_DONE)
#define USBPD_IS_RX_DONE(port)           \
	IS_MASK_SET(IT83XX_USBPD_ISR(port), USBPD_REG_MASK_MSG_RX_DONE)

enum usbpd_port {
	USBPD_PORT_A,
	USBPD_PORT_B,
	USBPD_PORT_COUNT,
};

enum usbpd_cc_pin {
	USBPD_CC_PIN_1,
	USBPD_CC_PIN_2,
};

enum usbpd_ufp_volt_status {
	USBPD_UFP_STATE_SNK_OPEN = 0,
	USBPD_UFP_STATE_SNK_DEF  = 1,
	USBPD_UFP_STATE_SNK_1_5  = 3,
	USBPD_UFP_STATE_SNK_3_0  = 7,
};

enum usbpd_dfp_volt_status {
	USBPD_DFP_STATE_SRC_RA   = 0,
	USBPD_DFP_STATE_SRC_RD   = 1,
	USBPD_DFP_STATE_SRC_OPEN = 3,
};

enum usbpd_power_role {
	USBPD_POWER_ROLE_CONSUMER,
	USBPD_POWER_ROLE_PROVIDER,
	USBPD_POWER_ROLE_CONSUMER_PROVIDER,
	USBPD_POWER_ROLE_PROVIDER_CONSUMER,
};

struct usbpd_header {
	uint8_t msg_type     : 4;
	uint8_t reserved     : 1;
	uint8_t port_role    : 1;
	uint8_t spec_ver     : 2;
	uint8_t power_role   : 1;
	uint8_t msg_id       : 3;
	uint8_t data_obj_num : 3;
	uint8_t reserved2    : 1;
};

void chip_pd_irq(enum usbpd_port port);
void chip_pd_init(enum usbpd_port port, int role);
void chip_pd_set_cc(enum usbpd_port port, int pull);
void chip_pd_set_power_role(enum usbpd_port port, int power_role);
void chip_pd_set_data_role(enum usbpd_port port, int pd_role);
void chip_pd_enable_vconn(enum usbpd_port port, int enabled);
void chip_pd_send_bist_mode2_pattern(enum usbpd_port port);
void chip_pd_select_polarity(enum usbpd_port port, enum usbpd_cc_pin cc_pin);
void chip_pd_rx_data(enum usbpd_port port, int *header, uint32_t *buf);
enum tcpc_transmit_complete chip_pd_send_hw_reset(
	enum usbpd_port port,
	enum tcpm_transmit_type type);
enum tcpc_transmit_complete chip_pd_tx_data(
	enum usbpd_port port,
	enum tcpm_transmit_type type,
	uint8_t msg_type,
	uint8_t length,
	const uint32_t *buf);
enum tcpc_cc_voltage_status chip_pd_get_cc(
	enum usbpd_port port,
	enum usbpd_cc_pin cc_pin);

#endif /* __CROS_EC_USB_PD_PHY_CHIP_H */
