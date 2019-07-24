/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MAX32660 I2C communications interface API */

#ifndef _I2C_API_H__
#define _I2C_API_H__

#include <stdint.h>
#include "common.h"
#include "ec_commands.h"
#include "i2c_regs.h"

/***** Definitions *****/

// I2C Speed Modes
typedef enum {
	I2C_STD_MODE = 100000,       //!< 100KHz Bus Speed
	I2C_FAST_MODE = 400000,      //!< 400KHz Bus Speed
	I2C_FASTPLUS_MODE = 1000000, //!< 1MHz   Bus Speed
	I2C_HS_MODE = 3400000        //!< 3.4MHz Bus Speed
} i2c_speed_t;

// I2C Transfer Direction
typedef enum {
	I2C_TRANSFER_DIRECTION_MASTER_WRITE = 0,
	I2C_TRANSFER_DIRECTION_MASTER_READ = 1,
	I2C_TRANSFER_DIRECTION_NONE = 2
} i2c_transfer_direction_t;

// Enable/Disable TXFIFO Autoflush mode
typedef enum {
	I2C_AUTOFLUSH_ENABLE = 0,
	I2C_AUTOFLUSH_DISABLE = 1
} i2c_autoflush_disable_t;

// Available transaction states for I2C Master
typedef enum {
	I2C_MASTER_IDLE = 1,
	I2C_MASTER_START = 2,
	I2C_MASTER_WRITE_COMPLETE = 3,
	I2C_MASTER_READ_COMPLETE = 4,
	I2C_MASTER_ERROR = EC_ERROR_UNKNOWN
} i2c_master_state_t;

// Available transaction states for I2C Slave
typedef enum {
	I2C_SLAVE_ADDR_MATCH = 1,
	I2C_SLAVE_WRITE_COMPLETE = 2,
	I2C_SLAVE_READ_COMPLETE = 3,
	I2C_SLAVE_ERROR = EC_ERROR_UNKNOWN
} i2c_slave_state_t;

// I2C Transaction request.
typedef struct i2c_req i2c_req_t;
struct i2c_req {

	uint8_t addr;                  /**     I2C 7-bit Address left aligned, bit 7 to bit 1.
                                     *     Only supports 7-bit addressing. LSb of the given address
                                     *     will be used as the read/write bit, the addr will 
                                     *     not be shifted. Used for both masterand 
                                     *     slave transactions.
                                     */
	const uint8_t *tx_data;             ///< Data for mater write/slave read.
	uint8_t *rx_data;                   ///< Data for master read/slave write.
	unsigned tx_len;                    ///< Length of tx data.
	unsigned rx_len;                    ///< Length of rx.
	unsigned tx_num;                    ///< Number of tx bytes sent.
	unsigned rx_num;                    ///< Number of rx bytes sent.
	i2c_transfer_direction_t direction; ///< For master:  sets direction bit in address. For slave: direction of request from master.

	/**
     * 0 to send a stop bit at the end of the transaction, 
     *   otherwise send a restart. Only used in master trasnactions.
     */
	int restart;                     /*  Restart or stop bit indicator. 
                                     *    0 to send a stop bit at the end of the transaction
                                     *    Non-zero to send a restart at end of the transaction
                                     *    Only used for Master transactions.
                                     */
	i2c_autoflush_disable_t sw_autoflush_disable; ///< Enable/Disable autoflush.

	/* driver status */
	enum ec_status driver_status;

};

/***** Function Prototypes *****/

/**
 * Initialize and enable I2C.
 *    i2c     Pointer to I2C peripheral registers.
 *    i2cspeed desired speed (I2C mode)
 *    sys_cfg System configuration object
 *    #E_NO_ERROR if everything is successful, 
 *             MXC_Error_Codes if an error occurred.
 */
int I2C_Api_Init(mxc_i2c_regs_t *i2c, i2c_speed_t i2cspeed);

/**
 * Master write data. Will block until transaction is complete.
 * i2c         Pointer to I2C regs.
 * addr        I2C 7-bit Address left aligned, bit 7 to bit 1.
 *             Only supports 7-bit addressing. LSb of the given address
 *             will be used as the read/write bit, the \p addr <b>will 
 *             not be shifted. Used for both master and 
 *             slave transactions.                              
 * data        Data to be written.
 * len         Number of bytes to Write.
 * restart     0 to send a stop bit at the end of the transaction, 
               otherwise send a restart.
 * Bytes transacted if everything is successful, 
 *             MXC_Error_Codes if an error occurred.
 */
int I2C_Api_MasterWrite(mxc_i2c_regs_t *i2c, uint8_t addr, int start, int stop, const uint8_t *data, int len, int restart);

/**
 * Master read data. Will block until transaction is complete.
 * i2c         Pointer to I2C regs.
 * addr        I2C 7-bit Address left aligned, bit 7 to bit 1.
 *             Only supports 7-bit addressing. LSb of the given address
 *             will be used as the read/write bit, the addr will 
 *             not be shifted. Used for both master and 
 *             slave transactions.  
 * data        Data to be written.
 * len         Number of bytes to Write.
 * restart     0 to send a stop bit at the end of the transaction, 
               otherwise send a restart.
 * Bytes transacted if everything is successful, MXC_Error_Codes if an error occurred.
 */
int I2C_Api_MasterRead(mxc_i2c_regs_t *i2c, uint8_t addr, int start, int stop, uint8_t *data, int len, int restart);

/**
 * Slave Read and Write Asynchronous.
 * i2c         Pointer to I2C regs.
 * req         Request for an I2C transaction.
 * #E_NO_ERROR if everything is successful, MXC_Error_Codes if an error occurred.
 */
//int I2C_Api_SlaveAsync(mxc_i2c_regs_t *i2c, i2c_req_t *req);

int I2C_Api_SlaveAsync(mxc_i2c_regs_t *i2c, i2c_req_t *req);
/**
 * I2C interrupt handler.
 * This function should be called by the application from the interrupt
 * handler if I2C interrupts are enabled. Alternately, this function
 * can be periodically called by the application if I2C interrupts are
 * disabled.
 * i2c         Base address of the I2C module.
 */
void I2C_Api_Handler(mxc_i2c_regs_t *i2c);

/**
 * Drain all of the data in the TXFIFO.
 * i2c     Pointer to I2C regs.
 */
void I2C_Api_DrainTX(mxc_i2c_regs_t *i2c);

#endif /* _I2C_API_H__ */
