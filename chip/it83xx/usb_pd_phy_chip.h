/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USB_PD_PHY_CHIP_H
#define __CROS_EC_USB_PD_PHY_CHIP_H

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
#define USBPD_ENABLE_VCONN(port)             \
	SET_MASK(IT83XX_USBPD_PDCSR(port),   \
		USBPD_REG_MASK_VCONN_ENABLE)
#define USBPD_DISABLE_VCONN(port)            \
	CLEAR_MASK(IT83XX_USBPD_PDCSR(port), \
		USBPD_REG_MASK_VCONN_ENABLE)
#define USBPD_START(port)                    \
	CLEAR_MASK(IT83XX_USBPD_CCGCR(port), \
		USBPD_REG_MASK_DISABLE_CC)
#define USBPD_DISABLE_VCONN(port)            \
	CLEAR_MASK(IT83XX_USBPD_PDCSR(port), \
		USBPD_REG_MASK_VCONN_ENABLE)
#define USBPD_ENABLE_SEND_BIST_MODE_2(port)  \
	SET_MASK(IT83XX_USBPD_MTSR0(port),   \
		USBPD_REG_MASK_SEND_BIST_MODE_2)
#define USBPD_DISABLE_SEND_BIST_MODE_2(port) \
	CLEAR_MASK(IT83XX_USBPD_MTSR0(port), \
		USBPD_REG_MASK_SEND_BIST_MODE_2)
#define USBPD_ENABLE_STAT_VBUS_5V(port)      \
	SET_MASK(IT83XX_USBPD_PDCSR(port),   \
		USBPD_REG_MASK_VBUS_STAT_5V_ENABLE)
#define USBPD_DISABLE_STAT_VBUS_5V(port)     \
	CLEAR_MASK(IT83XX_USBPD_PDCSR(port), \
		USBPD_REG_MASK_VBUS_STAT_5V_ENABLE)
#define USBPD_ENABLE_SOP_CABLE(port)         \
	SET_MASK(IT83XX_USBPD_PDMSR(port),   \
		(USBPD_REG_MASK_SOP_ENABLE | \
		USBPD_REG_MASK_SOPP_ENABLE | \
		USBPD_REG_MASK_SOPPP_ENABLE))
#define USBPD_DISABLE_SOP_CABLE(port)         \
	CLEAR_MASK(IT83XX_USBPD_PDMSR(port),  \
		(USBPD_REG_MASK_SOPP_ENABLE | \
		USBPD_REG_MASK_SOPPP_ENABLE))

/* macros for get */
#define USBPD_GET_POWER_ROLE(port)              \
	(IT83XX_USBPD_PDMSR(port) & 0x1)
#define USBPD_DATA_ROLE(port)                   \
	((IT83XX_USBPD_PDMSR(port) >> 2) & 0x1)
#define USBPD_GET_TX_SOP_TYPE(port)             \
	((IT83XX_USBPD_MTSR1(port) >> 4) & 0x3)
#define USBPD_GET_RX_SOP_TYPE(port)             \
	((IT83XX_USBPD_MRSR(port) >> 4) & 0x3)
#define USBPD_GET_CC1_PULL_REGISTER_SELECTION(port) \
	(IT83XX_USBPD_CCGCR(port) & (1 << 1))
#define USBPD_GET_CC2_PULL_REGISTER_SELECTION(port) \
	(IT83XX_USBPD_BMCSR(port) & (1 << 3))
#define USBPD_GET_PULL_CC_SELECTION(port)       \
	(IT83XX_USBPD_CCGCR(port) & 0x1)

/* macros for check */
#define USBPD_IS_TX_ERR(port)       \
	IS_MASK_SET(IT83XX_USBPD_MTCR(port), USBPD_REG_MASK_TX_ERR_STAT)
#define USBPD_IS_TX_DISCARD(port)   \
	IS_MASK_SET(IT83XX_USBPD_MTCR(port), USBPD_REG_MASK_TX_DISCARD_STAT)
#define USBPD_IS_SNIFFER_MODE(port) \
	IS_MASK_SET(IT83XX_USBPD_GCR(port), USBPD_REG_MASK_SNIFFER_MODE)

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
	USBPD_UFP_STATE_SNK_DEF = 1,
	USBPD_UFP_STATE_SNK_1_5 = 3,
	USBPD_UFP_STATE_SNK_3_0 = 7,
};

enum usbpd_dfp_volt_status {
	USBPD_DFP_STATE_SRC_RA = 0,
	USBPD_DFP_STATE_SRC_RD = 0x1,
	USBPD_DFP_STATE_SRC_OPEN = 0x3,
};

enum usbpd_result {
	USBPD_RESULT_SUCC,
	USBPD_RESULT_FAIL,
	USBPD_RESULT_TX_DISCARD,
	USBPD_RESULT_SKIP,
	USBPD_RESULT_NODATA,
	USBPD_RESULT_BUSY,
	USBPD_RESULT_DATA_ERROR,
	USBPD_RESULT_RESET,
	USBPD_RESULT_DISCONNECT,
};

enum usbpd_power_role {
	USBPD_POWER_ROLE_CONSUMER,
	USBPD_POWER_ROLE_PROVIDER,
	USBPD_POWER_ROLE_CONSUMER_PROVIDER,
	USBPD_POWER_ROLE_PROVIDER_CONSUMER,
};

enum usbpd_operation_role {
	USBPD_OPERATION_ROLE_UFP,
	USBPD_OPERATION_ROLE_DFP,
	USBPD_OPERATION_ROLE_DRP_UFP,
	USBPD_OPERATION_ROLE_DRP_DFP,
};

enum usbpd_sop_type {
	USBPD_SOP_TYPE_SOP,
	USBPD_SOP_TYPE_SOP_P,
	USBPD_SOP_TYPE_SOP_PP,
	USBPD_SOP_TYPE_CNT,
};

enum usbpd_reset_type {
	USBPD_RESET_TYPE_HARD,
	USBPD_RESET_TYPE_CABLE,
	USBPD_RESET_TYPE_CNT,
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
enum tcpc_transmit_complete chip_pd_send_hw_reset(
	enum usbpd_port port,
	enum usbpd_reset_type reset_type);
enum usbpd_result chip_pd_rx_data(
	enum usbpd_port port,
	enum usbpd_sop_type *sop_type,
	int *header,
	uint32_t *buf);
enum tcpc_transmit_complete chip_pd_tx_data(
	enum usbpd_port port,
	enum usbpd_sop_type sop_type,
	uint8_t msg_type,
	uint8_t length,
	const uint32_t *buf);
enum tcpc_cc_voltage_status chip_pd_get_cc(
	enum usbpd_port port,
	enum usbpd_cc_pin cc_pin);

#endif /* __CROS_EC_USB_PD_PHY_CHIP_H */
