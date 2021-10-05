/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 *
 * @brief Backend API for TCPC emulator
 */

#ifndef __EMUL_TCPC_H
#define __EMUL_TCPC_H

#include <emul.h>
#include <drivers/i2c.h>
#include <drivers/i2c_emul.h>
#include <usb_pd_tcpm.h>

/**
 * @brief TCPC emulator backend API
 * @defgroup tcpc_emul TCPC emulator
 * @{
 *
 * TCPC emulator supports access to its registers using I2C messages.
 * It follows Type-C Port Controller Interface Specification. It is possible
 * to use this emulator as base for implementation of specific TCPC emulator.
 * Emulator allows to set callbacks on change of CC status or transmitting
 * message to implement partner emulator. There is also callback used to
 * inform about alert line state change.
 * Application may alter emulator state:
 *
 * - call @ref tcpc_emul_set_reg, @ref tcpc_emul_set_reg16,
 *   @ref tcpc_emul_get_reg and @ref tcpc_emul_get_reg16 to set and get value
 *   of TCPC registers
 * - call tcpc_emul_set_err_* to change emulator behaviour on inadvisable driver
 *   behaviour
 * - call functions from emul_common_i2c.h to setup custom handlers for I2C
 *   messages
 * - call @ref tcpc_emul_add_rx_msg to setup received SOP messages
 * - call @ref tcpc_emul_get_tx_msg to examine sended message
 * - call @ref tcpc_emul_set_rev to set revision of emulated TCPC
 */

/**
 * Number of emulated register. This include vendor registers defined in TCPCI
 * specification
 */
#define TCPC_EMUL_REG_COUNT		0x100

/** SOP message structure */
struct tcpc_emul_msg {
	/** Pointer to buffer for header and message */
	uint8_t *buf;
	/** Number of bytes in buf */
	int cnt;
	/** Type of message (SOP, SOP', etc) */
	uint8_t type;
	/** Index used to mark accessed byte */
	int idx;
	/** Pointer to optional second message */
	struct tcpc_emul_msg *next;
};

/**
 * @brief Function type that is used by TCPC emulator to provide information
 *        about alert line state
 *
 * @param emul Pointer to emulator
 * @param alert State of alert line (0 low, 1 high)
 * @param data Pointer to custom function data
 */
typedef void (*tcpc_emul_alert_state_func)(const struct emul *emul, int alert,
					   void *data);

/** Response from TCPC specific device operations */
enum tcpc_emul_ops_resp {
	TCPC_EMUL_CONTINUE = 0,
	TCPC_EMUL_DONE,
	TCPC_EMUL_ERROR
};

/** TCPC specific device operations. Not all of them needs to be implemented. */
struct tcpc_emul_dev_ops {
	/**
	 * @brief Function called for each byte of read message
	 *
	 * @param emul Pointer to TCPC emulator
	 * @param ops Pointer to device operations structure
	 * @param reg First byte of last write message
	 * @param val Pointer where byte to read should be stored
	 * @param bytes Number of bytes already readded
	 *
	 * @return TCPC_EMUL_CONTINUE to continue with default handler
	 * @return TCPC_EMUL_DONE to immedietly return success
	 * @return TCPC_EMUL_ERROR to immedietly return error
	 */
	enum tcpc_emul_ops_resp (*read_byte)(const struct emul *emul,
					const struct tcpc_emul_dev_ops *ops,
					int reg, uint8_t *val, int bytes);

	/**
	 * @brief Function called for each byte of write message
	 *
	 * @param emul Pointer to TCPC emulator
	 * @param ops Pointer to device operations structure
	 * @param reg First byte of write message
	 * @param val Received byte of write message
	 * @param bytes Number of bytes already received
	 *
	 * @return TCPC_EMUL_CONTINUE to continue with default handler
	 * @return TCPC_EMUL_DONE to immedietly return success
	 * @return TCPC_EMUL_ERROR to immedietly return error
	 */
	enum tcpc_emul_ops_resp (*write_byte)(const struct emul *emul,
					const struct tcpc_emul_dev_ops *ops,
					int reg, uint8_t val, int bytes);

	/**
	 * @brief Function called on the end of write message
	 *
	 * @param emul Pointer to TCPC emulator
	 * @param ops Pointer to device operations structure
	 * @param reg Register which is written
	 * @param msg_len Length of handled I2C message
	 *
	 * @return TCPC_EMUL_CONTINUE to continue with default handler
	 * @return TCPC_EMUL_DONE to immedietly return success
	 * @return TCPC_EMUL_ERROR to immedietly return error
	 */
	enum tcpc_emul_ops_resp (*handle_write)(const struct emul *emul,
					const struct tcpc_emul_dev_ops *ops,
					int reg, int msg_len);

	/**
	 * @brief Function called on reset
	 *
	 * @param emul Pointer to TCPC emulator
	 * @param ops Pointer to device operations structure
	 */
	void (*reset)(const struct emul *emul, struct tcpc_emul_dev_ops *ops);
};

/** TCPC partner operations. Not all of them needs to be implemented. */
struct tcpc_emul_partner_ops {
	/**
	 * @brief Function called when TCPM wants to transmit message to partner
	 *        connected to TCPC
	 *
	 * @param emul Pointer to TCPC emulator
	 * @param ops Pointer to partner operations structure
	 * @param tx_msg Pointer to TX message buffer
	 * @param type Type of message
	 * @param retry Count of retries
	 */
	void (*transmit)(const struct emul *emul,
			 const struct tcpc_emul_partner_ops *ops,
			 const struct tcpc_emul_msg *tx_msg,
			 enum tcpci_msg_type type,
			 int retry);

	/**
	 * @brief Function called when control settings change to allow partner
	 *        to react
	 *
	 * @param emul Pointer to TCPC emulator
	 * @param ops Pointer to partner operations structure
	 */
	void (*control_change)(const struct emul *emul,
			       const struct tcpc_emul_partner_ops *ops);
};

/**
 * @brief Get i2c_emul for TCPC emulator
 *
 * @param emul Pointer to TCPC emulator
 *
 * @return Pointer to I2C TCPC emulator
 */
struct i2c_emul *tcpc_emul_get_i2c_emul(const struct emul *emul);

/**
 * @brief Set value of given register of TCPC
 *
 * @param emul Pointer to TCPC emulator
 * @param reg Register address which value will be changed
 * @param val New value of the register
 *
 * @return 0 on success
 * @return -EINVAL when register is out of range defined in TCPCI specification
 */
int tcpc_emul_set_reg(const struct emul *emul, int reg, uint16_t val);

/**
 * @brief Get value of given register of TCPC
 *
 * @param emul Pointer to TCPC emulator
 * @param reg Register address
 *
 * @return Value of the register
 */
uint16_t tcpc_emul_get_reg(const struct emul *emul, int reg);

/**
 * @brief Add up to two SOP RX messages
 *
 * @param emul Pointer to TCPC emulator
 * @param rx_msg Pointer to message that is added
 * @param alarm Select if alarm register should be updated
 *
 * @return 0 on success
 * @return Negative on error
 */
int tcpc_emul_add_rx_msg(const struct emul *emul, struct tcpc_emul_msg *rx_msg,
			 bool alert);

/**
 * @brief Get SOP TX message to examine what was sended by TCPM
 *
 * @param emul Pointer to TCPC emulator
 *
 * @return Pointer to TX message
 */
struct tcpc_emul_msg *tcpc_emul_get_tx_msg(const struct emul *emul);

/**
 * @brief Set TCPC revision in PD_INT_REV register
 *
 * @param emul Pointer to TCPC emulator
 * @param rev Requested revision (1 or 2)
 */
void tcpc_emul_set_rev(const struct emul *emul, int rev);

/**
 * @brief Set if error should be generated when read only register is being
 *        written
 *
 * @param emul Pointer to TCPC emulator
 * @param set Check for this error
 */
void tcpc_emul_set_err_on_ro_write(const struct emul *emul, bool set);

/**
 * @brief Set if error should be generated when reserved bits of register are
 *        not set to 0 on write I2C message
 *
 * @param emul Pointer to TCPC emulator
 * @param set Check for this error
 */
void tcpc_emul_set_err_on_rsvd_write(const struct emul *emul, bool set);

/**
 * @brief Set if error should be generated when in one I2C read message more
 *        than one register is accessed
 *
 * @param emul Pointer to TCPC emulator
 * @param set Check for this error
 */
void tcpc_emul_set_err_on_sequential_read(const struct emul *emul, bool set);

/**
 * @brief Set if error should be generated when in one I2C write message more
 *        than one register is accessed
 *
 * @param emul Pointer to TCPC emulator
 * @param set Check for this error
 */
void tcpc_emul_set_err_on_sequential_write(const struct emul *emul, bool set);

/**
 * @brief Set function that is called when alert line could change
 *
 * @param emul Pointer to TCPC emulator
 * @param func Pointer to alert function
 * @param func_data User data passed on call of alert function
 */
void tcpc_emul_set_alert_callback(const struct emul *emul,
				  tcpc_emul_alert_state_func func,
				  void *func_data);

/**
 * @brief Set callbacks for specific TCPC device emulator
 *
 * @param emul Pointer to TCPC emulator
 * @param dev_ops Pointer to callbacks
 */
void tcpc_emul_set_dev_ops(const struct emul *emul,
			   struct tcpc_emul_dev_ops *dev_ops);

/**
 * @brief Set callbacks for TCPC partner
 *
 * @param emul Pointer to TCPC emulator
 * @param partner_ops Pointer to callbacks
 */
void tcpc_emul_set_partner_ops(const struct emul *emul,
			       struct tcpc_emul_partner_ops *partner_ops);

/**
 * @}
 */

#endif /* __EMUL_TCPC */

