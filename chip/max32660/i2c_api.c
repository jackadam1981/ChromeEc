/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MAX32660 I2C API for Chrome EC */

#include <stddef.h>
#include <stdint.h>
#include "common.h"
#include "registers.h"
#include "i2c_api.h"
#include "system.h"
#include "gcr_regs.h"

/* **** Definitions **** */
#define I2C_ERROR (MXC_F_I2C_INT_FL0_ARB_ER | MXC_F_I2C_INT_FL0_TO_ER | MXC_F_I2C_INT_FL0_ADDR_NACK_ER |       \
									 MXC_F_I2C_INT_FL0_DATA_ER | MXC_F_I2C_INT_FL0_DO_NOT_RESP_ER | MXC_F_I2C_INT_FL0_START_ER | \
									 MXC_F_I2C_INT_FL0_STOP_ER)
#define MASTER 1
#define SLAVE 0

/* For high speed mode, if the I2C bus capacitance is greater than 100pF, set this value to ((capacitance - 100) / 3).
   Otherwise leave it at 0. */
#define HS_SCALE_FACTOR (0)

#define T_LOW_MIN (160 + (160 * HS_SCALE_FACTOR / 100)) /* tLOW minimum in nanoseconds */
#define T_HIGH_MIN (60 + (60 * HS_SCALE_FACTOR / 100))	/* tHIGH minimum in nanoseconds */
#define T_R_MAX_HS (40 + (40 * HS_SCALE_FACTOR / 100))	/* tR maximum for high speed mode in nanoseconds */
#define T_F_MAX_HS (40 + (40 * HS_SCALE_FACTOR / 100))	/* tF maximum for high speed mode in nanoseconds */
#define T_AF_MIN (10 + (10 * HS_SCALE_FACTOR / 100))		/* tAF minimun in nanoseconds */

// Saves the state of the non-blocking requests
typedef struct {
	i2c_req_t *req;
	i2c_master_state_t master_state;
	i2c_slave_state_t slave_state;
	uint8_t num_wr; // keep track of number of bytes loaded in the fifo during slave transmit
} i2c_req_state_t;

static i2c_req_state_t states[MXC_I2C_INSTANCES];
static int rx_remain = 0, tx_remain = 0;
static int slave_rx_remain = 0, slave_tx_remain = 0;

/* **** Function Prototypes **** */
static void I2C_Api_MasterHandler(mxc_i2c_regs_t *i2c);
static void I2C_Api_Recover(mxc_i2c_regs_t *i2c);

/* ************************************************************************** */
static int I2C_Api_Setspeed(mxc_i2c_regs_t *i2c, i2c_speed_t i2cspeed)
{
	uint32_t ticks, ticks_lo, ticks_hi;

	if (i2cspeed == I2C_HS_MODE) {

		uint32_t sys_freq, tPCLK, targBusFreq, tSCLmin, cklMin, ckhMin, ckh_cklMin;

		/* Compute dividers for high speed mode. */
		sys_freq = PeripheralClock;

		tPCLK = 1000000 / (sys_freq / 1000);

		targBusFreq = i2cspeed - ((i2cspeed / 2) * HS_SCALE_FACTOR / 100);
		if (targBusFreq < 1000) {
			return EC_ERROR_INVAL;
		}

		tSCLmin = 1000000 / (targBusFreq / 1000);
		cklMin = ((T_LOW_MIN + T_F_MAX_HS + (tPCLK - 1) - T_AF_MIN) / tPCLK) - 1;
		ckhMin = ((T_HIGH_MIN + T_R_MAX_HS + (tPCLK - 1) - T_AF_MIN) / tPCLK) - 1;
		ckh_cklMin = ((tSCLmin + (tPCLK - 1)) / tPCLK) - 2;

		ticks_lo = (cklMin > (ckh_cklMin - ckhMin)) ? (cklMin) : (ckh_cklMin - ckhMin);
		ticks_hi = ckhMin;

		if ((ticks_lo > (MXC_F_I2C_HS_CLK_HS_CLK_LO >> MXC_F_I2C_HS_CLK_HS_CLK_LO_POS)) ||
				(ticks_hi > (MXC_F_I2C_HS_CLK_HS_CLK_HI >> MXC_F_I2C_HS_CLK_HS_CLK_HI_POS))) {
			return EC_ERROR_INVAL;
		}

		/* Write results to destination registers. */
		i2c->hs_clk = (ticks_lo << MXC_F_I2C_HS_CLK_HS_CLK_LO_POS) | (ticks_hi << MXC_F_I2C_HS_CLK_HS_CLK_HI_POS);

		/* Still need to load dividers for the preamble that each high-speed transaction starts with.
           Switch setting to fast mode and fall out of if statement. */
		i2cspeed = I2C_FAST_MODE;
	}

	/* Get the number of periph clocks needed to achieve selected speed. */
	ticks = PeripheralClock / i2cspeed;

	/* For a 50% duty cycle, half the ticks will be spent high and half will be low. */
	ticks_hi = (ticks >> 1) - 1;
	ticks_lo = (ticks >> 1) - 1;

	/* Account for rounding error in odd tick counts. */
	if (ticks & 1) {
		ticks_hi++;
	}

	/* Will results fit into 9 bit registers?  (ticks_hi will always be >= ticks_lo.  No need to check ticks_lo.) */
	if (ticks_hi > 0x1FF) {
		return EC_ERROR_INVAL;
	}

	/* 0 is an invalid value for the destination registers. (ticks_hi will always be >= ticks_lo.  No need to check ticks_hi.) */
	if (ticks_lo == 0) {
		return EC_ERROR_INVAL;
	}

	/* Write results to destination registers. */
	i2c->clk_lo = ticks_lo;
	i2c->clk_hi = ticks_hi;

	return EC_SUCCESS;
}

/* ************************************************************************** */
int I2C_Api_Init(mxc_i2c_regs_t *i2c, i2c_speed_t i2cspeed)
{
	int idx = MXC_I2C_GET_IDX(i2c);

	// Enable the peripheral clock
	if (i2c == MXC_I2C0) {
		MXC_GCR->perckcn0 &= ~(MXC_F_GCR_PERCKCN0_I2C0D);
	} else if (i2c == MXC_I2C1) {
		MXC_GCR->perckcn0 &= ~(MXC_F_GCR_PERCKCN0_I2C1D);
	} 

	// Always disable the HW autoflush on data NACK and let the SW handle the flushing.
	i2c->tx_ctrl0 |= 0x20;

	states[idx].num_wr = 0;

	i2c->ctrl = 0;										 // clear configuration bits
	i2c->ctrl = MXC_F_I2C_CTRL_I2C_EN; // Enable I2C
	i2c->master_ctrl = 0;							 // clear master configuration bits
	i2c->status = 0;									 // clear status bits

	/* If either SDA or SCL is already low, there is a problem.
     * Try reclaiming the bus by sending clocks until we have control of the SDA line.
     * Follow procedure defined in i2c spec.
     */
	if ((i2c->ctrl & (MXC_F_I2C_CTRL_SCL | MXC_F_I2C_CTRL_SDA)) !=
			(MXC_F_I2C_CTRL_SCL | MXC_F_I2C_CTRL_SDA)) {

		int i, have_control;

		// Set SCL/SDA as software controlled.
		i2c->ctrl |= MXC_F_I2C_CTRL_SW_OUT_EN;

		// Try to get control of SDA.
		for (i = 0; i < 16; i++) {
			have_control = 1;

			// Drive SCL low and check its state.
			i2c->ctrl &= ~(MXC_F_I2C_CTRL_SCL_OUT);
			//mxc_delay(MXC_DELAY_USEC(5));
			if ((i2c->ctrl & MXC_F_I2C_CTRL_SCL) == MXC_F_I2C_CTRL_SCL) {
				have_control = 0;
			}

			// Drive SDA low and check its state.
			i2c->ctrl &= ~(MXC_F_I2C_CTRL_SDA_OUT);
			//mxc_delay(MXC_DELAY_USEC(5));
			if ((i2c->ctrl & MXC_F_I2C_CTRL_SDA) == MXC_F_I2C_CTRL_SDA) {
				have_control = 0;
			}

			// Release SDA and check its state.
			i2c->ctrl |= (MXC_F_I2C_CTRL_SDA_OUT);
			//mxc_delay(MXC_DELAY_USEC(5));
			if ((i2c->ctrl & MXC_F_I2C_CTRL_SDA) != MXC_F_I2C_CTRL_SDA) {
				have_control = 0;
			}

			// Release SCL and check its state.
			i2c->ctrl |= (MXC_F_I2C_CTRL_SCL_OUT);
			//mxc_delay(MXC_DELAY_USEC(5));
			if ((i2c->ctrl & MXC_F_I2C_CTRL_SCL) != MXC_F_I2C_CTRL_SCL) {
				have_control = 0;
			}

			if (have_control) {
				// Issue stop
				// Drive SDA low.
				i2c->ctrl &= ~(MXC_F_I2C_CTRL_SDA_OUT);
				//mxc_delay(MXC_DELAY_USEC(5));
				// Release SDA.
				i2c->ctrl |= (MXC_F_I2C_CTRL_SDA_OUT);
				//mxc_delay(MXC_DELAY_USEC(5));
				break;
			}
		}

		if (!have_control) {
			return EC_ERROR_UNKNOWN;
		}
	}

	i2c->ctrl = 0;										 // clear configuration bits
	i2c->ctrl = MXC_F_I2C_CTRL_I2C_EN; // Enable I2C
	i2c->master_ctrl = 0;							 // clear master configuration bits
	i2c->status = 0;									 // clear status bits

	// Check for HS mode
	if (i2cspeed == I2C_HS_MODE) {
		i2c->ctrl |= MXC_F_I2C_CTRL_HS_MODE; // Enable HS mode
	}

	// Disable and clear interrupts
	i2c->int_en0 = 0;
	i2c->int_en1 = 0;
	i2c->int_fl0 = i2c->int_fl0;
	i2c->int_fl1 = i2c->int_fl1;

	i2c->timeout = 0x0;														// set timeout
	i2c->rx_ctrl0 |= MXC_F_I2C_RX_CTRL0_RX_FLUSH; // clear the RX FIFO
	i2c->tx_ctrl0 |= MXC_F_I2C_TX_CTRL0_TX_FLUSH; // clear the TX FIFO

	return I2C_Api_Setspeed(i2c, i2cspeed);
}

/* ************************************************************************** */
int I2C_Api_MasterWrite(mxc_i2c_regs_t *i2c, uint8_t addr, int start, int stop, const uint8_t *data, int len, int restart)
{
	int save_len = len;

	if (len == 0) {
		return EC_SUCCESS;
	}

	// Clear the interrupt flag
	i2c->int_fl0 = i2c->int_fl0;

	// Make sure the I2C has been initialized
	if (!(i2c->ctrl & MXC_F_I2C_CTRL_I2C_EN)) {
		return EC_ERROR_UNKNOWN;
	}

	// Enable master mode
	i2c->ctrl |= MXC_F_I2C_CTRL_MST;

	// Load FIFO with slave address for WRITE and as much data as we can
	while (i2c->status & MXC_F_I2C_STATUS_TX_FULL) {
	}

	if (start) {
		// load the slave address with write/read bit
		i2c->fifo = addr & ~(0x1);
	}

	while ((len > 0) && !(i2c->status & MXC_F_I2C_STATUS_TX_FULL)) {
		i2c->fifo = *data++;
		len--;
	}
	// Generate Start signal
	if (start) {
		i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_START;
	}

	// Write remaining data to FIFO
	while (len > 0) {
		// Check for errors
		if (i2c->int_fl0 & I2C_ERROR) {
			// Set the stop bit
			i2c->master_ctrl &= ~(MXC_F_I2C_MASTER_CTRL_RESTART);
			i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_STOP;
			return EC_ERROR_UNKNOWN;
		}

		if (!(i2c->status & MXC_F_I2C_STATUS_TX_FULL)) {
			i2c->fifo = *data++;
			len--;
		}
	}
	// Check if Repeated Start requested
	if (restart) {
		i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_RESTART;
	} else {
		if (stop) {
			i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_STOP;
		}
	}

	if (stop) {
		// Wait for Done
		while (!(i2c->int_fl0 & MXC_F_I2C_INT_FL0_DONE)) {
			// Check for errors
			if (i2c->int_fl0 & I2C_ERROR) {
				// Set the stop bit
				i2c->master_ctrl &= ~(MXC_F_I2C_MASTER_CTRL_RESTART);
				i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_STOP;
				return EC_ERROR_UNKNOWN;
			}
		}
		// Clear Done interrupt flag
		i2c->int_fl0 = MXC_F_I2C_INT_FL0_DONE;
	}

	// Wait for Stop
	if (stop) {
		if (!restart) {
			while (!(i2c->int_fl0 & MXC_F_I2C_INT_FL0_STOP)) {
				// Check for errors
				if (i2c->int_fl0 & I2C_ERROR) {
					// Set the stop bit
					i2c->master_ctrl &= ~(MXC_F_I2C_MASTER_CTRL_RESTART);
					i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_STOP;
					return EC_ERROR_UNKNOWN;
				}
			}
			// Clear stop interrupt flag
			i2c->int_fl0 = MXC_F_I2C_INT_FL0_STOP;
		}
	}

	// Check for errors
	if (i2c->int_fl0 & I2C_ERROR) {
		return EC_ERROR_UNKNOWN;
	}

	return save_len;
}

/* ************************************************************************** */
int I2C_Api_MasterRead(mxc_i2c_regs_t *i2c, uint8_t addr, int start, int stop, uint8_t *data, int len, int restart)
{
	int save_len = len;
	volatile int length = len;
	int interactive_receive_mode;

	if (len == 0) {
		return EC_SUCCESS;
	}

	if (len > 256) {
		return EC_ERROR_INVAL;
	}

	// Clear the interrupt flag
	i2c->int_fl0 = i2c->int_fl0;

	// Make sure the I2C has been initialized
	if (!(i2c->ctrl & MXC_F_I2C_CTRL_I2C_EN)) {
		return EC_ERROR_UNKNOWN;
	}

	// Enable master mode
	i2c->ctrl |= MXC_F_I2C_CTRL_MST;

	if (stop) {
		// Set receive count
		i2c->ctrl &= ~MXC_F_I2C_CTRL_RX_MODE;
		i2c->rx_ctrl1 = len;
		interactive_receive_mode = 0;
	} else {
		i2c->ctrl |= MXC_F_I2C_CTRL_RX_MODE;
		i2c->rx_ctrl1 = 1;
		interactive_receive_mode = 1;
	}

	// Load FIFO with slave address
	if (start) {
		i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_START;
		while (i2c->status & MXC_F_I2C_STATUS_TX_FULL) {
		}
		i2c->fifo = (addr | 1);
	}

	// Wait for all data to be received or error
	while (length > 0) {
		// Check for errors
		if (i2c->int_fl0 & I2C_ERROR) {
			// Set the stop bit
			i2c->master_ctrl &= ~(MXC_F_I2C_MASTER_CTRL_RESTART);
			i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_STOP;
			return EC_ERROR_UNKNOWN;
		}

		// if in interactive receive mode then ack each received byte
		if (interactive_receive_mode) {
			while (!(i2c->int_fl0 & MXC_F_I2C_INT_EN0_RX_MODE))
				;
			if (i2c->int_fl0 & MXC_F_I2C_INT_EN0_RX_MODE) {
				// read the data
				*data++ = i2c->fifo;
				length--;
				// clear the bit
				if (length != 1) {
					i2c->int_fl0 = MXC_F_I2C_INT_EN0_RX_MODE;
				}
			}
		} else {
			//while (i2c->status & MXC_F_I2C_STATUS_RX_EMPTY)
			//    ;
			if (!(i2c->status & MXC_F_I2C_STATUS_RX_EMPTY)) {
				*data++ = i2c->fifo;
				length--;
			}
		}
	}

	if (restart) {
		i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_RESTART;
	} else {
		if (stop) {
			i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_STOP;
		}
	}

	// Wait for Done
	if (stop) {
		while (!(i2c->int_fl0 & MXC_F_I2C_INT_FL0_DONE)) {
			// Check for errors
			if (i2c->int_fl0 & I2C_ERROR) {
				// Set the stop bit
				i2c->master_ctrl &= ~(MXC_F_I2C_MASTER_CTRL_RESTART);
				i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_STOP;
				return EC_ERROR_UNKNOWN;
			}
		}
		// Clear Done interrupt flag
		i2c->int_fl0 = MXC_F_I2C_INT_FL0_DONE;
	}

	// Wait for Stop
	if (!restart) {
		if (stop) {
			while (!(i2c->int_fl0 & MXC_F_I2C_INT_FL0_STOP)) {
				// Check for errors
				if (i2c->int_fl0 & I2C_ERROR) {
					// Set the stop bit
					i2c->master_ctrl &= ~(MXC_F_I2C_MASTER_CTRL_RESTART);
					i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_STOP;
					return EC_ERROR_UNKNOWN;
				}
			}
			// Clear Stop interrupt flag
			i2c->int_fl0 = MXC_F_I2C_INT_FL0_STOP;
		}
	}

	// Check for errors
	if (i2c->int_fl0 & I2C_ERROR) {
		return EC_ERROR_UNKNOWN;
	}

	return save_len;
}


/* ***************************************************************************/
static void I2C_Api_MasterHandler(mxc_i2c_regs_t *i2c)
{
	uint32_t int0;
	int i2c_num;
	i2c_req_t *req;

	i2c_num = MXC_I2C_GET_IDX(i2c);
	req = states[i2c_num].req;

	int0 = i2c->int_fl0;

	// Clear the interrupts
	i2c->int_fl0 = int0;

	if (int0 & I2C_ERROR) {
		// Disable all interrupts
		i2c->int_en0 = 0;
		// Set the stop bit
		i2c->master_ctrl &= ~(MXC_F_I2C_MASTER_CTRL_RESTART);
		i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_STOP;
		I2C_Api_Recover(i2c);
		req->driver_status = EC_ERROR_UNKNOWN;
		return;
	}

	// Check for STOP interrupt
	if ((int0 & MXC_F_I2C_INT_FL0_STOP)) {
		// Disable all interrupts
		i2c->int_en0 = 0;
		tx_remain = 0;
		rx_remain = 0;
		// store back the idle state
		states[i2c_num].master_state = I2C_MASTER_IDLE;
		I2C_Api_Recover(i2c);
		req->driver_status = EC_SUCCESS;
		return;
	}

	// Check for Done interrupt
	if (int0 & MXC_F_I2C_INT_FL0_DONE) {
		// Disable Done interrupt
		i2c->int_en0 &= ~(MXC_F_I2C_INT_EN0_DONE);
		return;
	}

	// Check transfer direction(R/W)
	if (req->direction == I2C_TRANSFER_DIRECTION_MASTER_READ) {
		if (states[i2c_num].master_state == I2C_MASTER_START) {
			if (rx_remain != 0) {
				if (rx_remain == req->rx_len) {
					//set rx count
					i2c->rx_ctrl1 = req->rx_len;
				}
				// Read out any data in the RX FIFO
				while ((rx_remain > 0) && !(i2c->status & MXC_F_I2C_STATUS_RX_EMPTY)) {
					*(req->rx_data)++ = i2c->fifo;
					req->rx_num++;
					rx_remain--;
				}
				// Set the RX threshold interrupt level
				if (rx_remain >= (MXC_I2C_FIFO_DEPTH - 1)) {
					i2c->rx_ctrl0 = ((i2c->rx_ctrl0 & ~(MXC_F_I2C_RX_CTRL0_RX_THRESH)) |
													 (MXC_I2C_FIFO_DEPTH - 1) << MXC_F_I2C_RX_CTRL0_RX_THRESH_POS);
				} else {
					i2c->rx_ctrl0 = ((i2c->rx_ctrl0 & ~(MXC_F_I2C_RX_CTRL0_RX_THRESH)) |
													 (rx_remain) << MXC_F_I2C_RX_CTRL0_RX_THRESH_POS);
				}
				// Enable RXTH interrupt and Error interrupts
				i2c->int_en0 |= (MXC_F_I2C_INT_EN0_RX_THRESH | I2C_ERROR);

				// Check for Errors if any
				if (i2c->int_fl0 & I2C_ERROR) {
					i2c->int_en0 = 0;
					// Set the stop bit
					i2c->master_ctrl &= ~(MXC_F_I2C_MASTER_CTRL_RESTART);
					i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_STOP;
					I2C_Api_Recover(i2c);
					req->driver_status = EC_ERROR_UNKNOWN;
					return;
				}
			} else {
				// store the current state of the master
				states[i2c_num].master_state = I2C_MASTER_READ_COMPLETE;
				// Disable RXTH interrupt
				i2c->int_en0 &= ~(MXC_F_I2C_INT_EN0_RX_THRESH);
				if (req->restart) {
					// set the done interrupt
					i2c->int_en0 |= MXC_F_I2C_INT_EN0_DONE;
				}
			}
		}

		if (states[i2c_num].master_state == I2C_MASTER_READ_COMPLETE) {
			// Check for Errors if any
			if (i2c->int_fl0 & I2C_ERROR) {
				i2c->int_en0 = 0;
				// Set the stop bit
				i2c->master_ctrl &= ~(MXC_F_I2C_MASTER_CTRL_RESTART);
				i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_STOP;
				I2C_Api_Recover(i2c);
				req->driver_status = EC_SUCCESS;
				return;
			}
			// Check if repeated start requested else generate stop
			if (!req->restart) {
				// Enable stop interrupt
				i2c->int_en0 |= MXC_F_I2C_INT_EN0_STOP;
				// Generate Stop signal
				i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_STOP;
			}
		}

	} else {
		if (states[i2c_num].master_state == I2C_MASTER_START) {
			if (tx_remain != 0) {
				// Fill the FIFO
				while ((tx_remain) && !(i2c->status & MXC_F_I2C_STATUS_TX_FULL)) {
					i2c->fifo = *(req->tx_data)++;
					req->tx_num++;
					tx_remain--;
				}
				// Set the TX threshold interrupt level
				if (tx_remain >= (MXC_I2C_FIFO_DEPTH - 1)) {
					i2c->tx_ctrl0 = ((i2c->tx_ctrl0 & ~(MXC_F_I2C_TX_CTRL0_TX_THRESH)) |
													 (MXC_I2C_FIFO_DEPTH - 1) << MXC_F_I2C_TX_CTRL0_TX_THRESH_POS);

				} else {
					i2c->tx_ctrl0 = ((i2c->tx_ctrl0 & ~(MXC_F_I2C_TX_CTRL0_TX_THRESH)) |
													 (tx_remain) << MXC_F_I2C_TX_CTRL0_TX_THRESH_POS);
				}
				// Enable TXTH interrupt and Error interrupts
				i2c->int_en0 |= (MXC_F_I2C_INT_EN0_TX_THRESH | I2C_ERROR);

				// Check for Errors if any
				if (i2c->int_fl0 & I2C_ERROR) {
					i2c->int_en0 = 0;
					// Set the stop bit
					i2c->master_ctrl &= ~(MXC_F_I2C_MASTER_CTRL_RESTART);
					i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_STOP;
					I2C_Api_Recover(i2c);
					req->driver_status = EC_ERROR_UNKNOWN;
					return;
				}
			} else {
				// store the current state of the master
				states[i2c_num].master_state = I2C_MASTER_WRITE_COMPLETE;
				// Disable TXTH interrupt
				i2c->int_en0 &= ~(MXC_F_I2C_INT_EN0_TX_THRESH);
				if (req->restart) {
					// set the done interrupt
					i2c->int_en0 |= MXC_F_I2C_INT_EN0_DONE;
				}
			}
		}

		if (states[i2c_num].master_state == I2C_MASTER_WRITE_COMPLETE) {
			// Check for Errors if any
			if (i2c->int_fl0 & I2C_ERROR) {
				i2c->int_en0 = 0;
				// Set the stop bit
				i2c->master_ctrl &= ~(MXC_F_I2C_MASTER_CTRL_RESTART);
				i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_STOP;
				I2C_Api_Recover(i2c);
				req->driver_status = EC_ERROR_UNKNOWN;
				return;
			}
			// Check if repeated start requested else generate stop
			if (!req->restart) {
				// Enable stop interrupt
				i2c->int_en0 |= MXC_F_I2C_INT_EN0_STOP;
				// Generate Stop signal
				i2c->master_ctrl |= MXC_F_I2C_MASTER_CTRL_STOP;
			}
		}
	}
}

/* ************************************************************************** */
int I2C_Api_SlaveAsync(mxc_i2c_regs_t *i2c, i2c_req_t *req)
{
	int i2c_num;
	i2c_num = MXC_I2C_GET_IDX(i2c);

	// Make sure the I2C has been initialized
	if (!(i2c->ctrl & MXC_F_I2C_CTRL_I2C_EN)) {
		return EC_ERROR_UNKNOWN;
	}

	states[i2c_num].req = req;

	// Disable master mode
	i2c->ctrl &= ~(MXC_F_I2C_CTRL_MST);
	// Set Slave Address
	i2c->slave_addr = (req->addr >> 1);

	// Clear the byte counters
	req->tx_num = 0;
	req->rx_num = 0;

	// Disable and clear the interrupts
	i2c->int_en0 = 0;
	i2c->int_en1 = 0;
	i2c->int_fl0 = i2c->int_fl0;
	i2c->int_fl1 = i2c->int_fl1;
	i2c->int_en0 = MXC_F_I2C_INT_EN0_ADDR_MATCH;

	return EC_SUCCESS;
}

/* ************************************************************************** */
static void I2C_Api_SlaveHandler(mxc_i2c_regs_t *i2c)
{
#ifdef I2C_SEND_BYTE_EC
	int i;
#endif
	uint32_t int0;
	int i2c_num;
	i2c_req_t *req;

	i2c_num = MXC_I2C_GET_IDX(i2c);
	req = states[i2c_num].req;

	// Check for an Address match
	if (i2c->int_fl0 & MXC_F_I2C_INT_FL0_ADDR_MATCH) {
		// Clear AMI and TXLOI
		i2c->int_fl0 |= MXC_F_I2C_INT_FL0_DONE;
		i2c->int_fl0 |= MXC_F_I2C_INT_FL0_ADDR_MATCH;
		i2c->int_fl0 |= MXC_F_I2C_INT_FL0_TX_LOCK_OUT;
		// Store the current state of the Slave
		states[i2c_num].slave_state = I2C_SLAVE_ADDR_MATCH;
		// Set the Done, Stop interrupt
		i2c->int_en0 |= MXC_F_I2C_INT_EN0_DONE | MXC_F_I2C_INT_EN0_STOP;
		/* Inhibit sleep mode when addressed until STOPF flag is set */
		disable_sleep(SLEEP_MASK_I2C_SLAVE);
	}

	// Check for errors
	int0 = i2c->int_fl0;
	// Clear the interrupts
	i2c->int_fl0 = int0;

	if (int0 & I2C_ERROR) {
		i2c->int_en0 = 0;
		// Calculate the number of bytes sent by the slave
		req->tx_num = states[i2c_num].num_wr - ((i2c->tx_ctrl1 & MXC_F_I2C_TX_CTRL1_TX_FIFO) >> MXC_F_I2C_TX_CTRL1_TX_FIFO_POS);

		if (!req->sw_autoflush_disable) {
			// Manually clear the TXFIFO
			i2c->tx_ctrl0 |= MXC_F_I2C_TX_CTRL0_TX_FLUSH;
		}
		states[i2c_num].num_wr = 0;
		I2C_Api_Recover(i2c);
		req->driver_status = EC_ERROR_UNKNOWN;
		return;
	}

	slave_rx_remain = req->rx_len - req->rx_num;
	slave_tx_remain = req->tx_len - states[i2c_num].num_wr;

	// Check for Stop interrupt
	if (int0 & MXC_F_I2C_INT_FL0_STOP) {
		if (req->direction == I2C_TRANSFER_DIRECTION_MASTER_WRITE) {
			// Read out any data in the RX FIFO
			while (!(i2c->status & MXC_F_I2C_STATUS_RX_EMPTY)) {
				*(req->rx_data)++ = i2c->fifo;
				req->rx_num++;
			}
		}

		// Disable all interrupts
		i2c->int_en0 = 0;
		// Calculate the number of bytes sent by the slave
		req->tx_num = states[i2c_num].num_wr - ((i2c->tx_ctrl1 & MXC_F_I2C_TX_CTRL1_TX_FIFO) >> MXC_F_I2C_TX_CTRL1_TX_FIFO_POS);
		slave_rx_remain = 0;
		slave_tx_remain = 0;
		if (!req->sw_autoflush_disable) {
			// Manually clear the TXFIFO
			i2c->tx_ctrl0 |= MXC_F_I2C_TX_CTRL0_TX_FLUSH;
		}
		I2C_Api_Recover(i2c);
		req->driver_status = EC_SUCCESS;

		req->direction = I2C_TRANSFER_DIRECTION_NONE;
		states[i2c_num].num_wr = 0;

		//
		// be ready to receive more data
		//
		req->rx_len = 128;
		// Clear the byte counters
		req->tx_num = 0;
		req->rx_num = 0;
		// Disable and clear the interrupts
		i2c->int_en0 = 0;
		i2c->int_en1 = 0;
		i2c->int_fl0 = i2c->int_fl0;
		i2c->int_fl1 = i2c->int_fl1;
		i2c->int_en0 = MXC_F_I2C_INT_EN0_ADDR_MATCH;

		/* No longer inhibit deep sleep after stop condition */
		enable_sleep(SLEEP_MASK_I2C_SLAVE);
		return;
	}

	// Check for DONE interrupt
	if (int0 & MXC_F_I2C_INT_FL0_DONE) {

		if (req->direction == I2C_TRANSFER_DIRECTION_MASTER_WRITE) {
			// Read out any data in the RX FIFO
			while (!(i2c->status & MXC_F_I2C_STATUS_RX_EMPTY)) {
				*(req->rx_data)++ = i2c->fifo;
				req->rx_num++;
			}
		}
		// Disable Done interrupt
		i2c->int_en0 &= ~(MXC_F_I2C_INT_EN0_DONE);
		// Calculate the number of bytes sent by the slave
		req->tx_num = states[i2c_num].num_wr - ((i2c->tx_ctrl1 & MXC_F_I2C_TX_CTRL1_TX_FIFO) >> MXC_F_I2C_TX_CTRL1_TX_FIFO_POS);
		slave_rx_remain = 0;
		slave_tx_remain = 0;
		if (!req->sw_autoflush_disable) {
			// Manually clear the TXFIFO
			i2c->tx_ctrl0 |= MXC_F_I2C_TX_CTRL0_TX_FLUSH;
		}
		req->driver_status = EC_SUCCESS;
		req->direction = I2C_TRANSFER_DIRECTION_NONE;
		states[i2c_num].num_wr = 0;
		return;
	}

	if (states[i2c_num].slave_state == I2C_SLAVE_ADDR_MATCH) {
		//
		// Check if Master Read has been called and if there is a tx_data buffer
		//
		if (i2c->ctrl & MXC_F_I2C_CTRL_READ) {
			req->direction = I2C_TRANSFER_DIRECTION_MASTER_READ;
			if (req->tx_data == NULL) {
				I2C_Api_Recover(i2c);
				req->driver_status = EC_ERROR_UNKNOWN;
				return;
			}
			if (slave_tx_remain != 0) {
				// Fill the FIFO
				while ((slave_tx_remain > 0) && !(i2c->status & MXC_F_I2C_STATUS_TX_FULL)) {
					i2c->fifo = *(req->tx_data)++;
					states[i2c_num].num_wr++;
					slave_tx_remain--;
				}
				// Set the TX threshold interrupt level
				if (slave_tx_remain >= (MXC_I2C_FIFO_DEPTH - 1)) {
					i2c->tx_ctrl0 = ((i2c->tx_ctrl0 & ~(MXC_F_I2C_TX_CTRL0_TX_THRESH)) |
													 (MXC_I2C_FIFO_DEPTH - 1) << MXC_F_I2C_TX_CTRL0_TX_THRESH_POS);

				} else {
					i2c->tx_ctrl0 = ((i2c->tx_ctrl0 & ~(MXC_F_I2C_TX_CTRL0_TX_THRESH)) |
													 (slave_tx_remain) << MXC_F_I2C_TX_CTRL0_TX_THRESH_POS);
				}
				// Enable TXTH interrupt and Error interrupts
				i2c->int_en0 |= (MXC_F_I2C_INT_EN0_TX_THRESH | I2C_ERROR);
				if (int0 & I2C_ERROR) {
					i2c->int_en0 = 0;
					// Calculate the number of bytes sent by the slave
					req->tx_num = states[i2c_num].num_wr - ((i2c->tx_ctrl1 & MXC_F_I2C_TX_CTRL1_TX_FIFO) >> MXC_F_I2C_TX_CTRL1_TX_FIFO_POS);
					if (!req->sw_autoflush_disable) {
						// Manually clear the TXFIFO
						i2c->tx_ctrl0 |= MXC_F_I2C_TX_CTRL0_TX_FLUSH;
					}
					states[i2c_num].num_wr = 0;
					I2C_Api_Recover(i2c);
					req->driver_status = EC_ERROR_UNKNOWN;

					return;
				}
			} else {
#ifdef I2C_SEND_BYTE_EC
				for (i = 0; i < 1; i++) {
					if (!(i2c->status & MXC_F_I2C_STATUS_TX_FULL)) {
						i2c->fifo = 0xec;
					}
				}
				// set tx threshold to zero
				i2c->tx_ctrl0 = ((i2c->tx_ctrl0 & ~(MXC_F_I2C_TX_CTRL0_TX_THRESH)) |
												 (0) << MXC_F_I2C_TX_CTRL0_TX_THRESH_POS);
				// Enable TXTH interrupt and Error interrupts
				i2c->int_en0 |= (MXC_F_I2C_INT_EN0_TX_THRESH | I2C_ERROR);

#endif
			}
		} else {
			//
			// Master Write has been called and if there is a rx_data buffer
			//
			req->direction = I2C_TRANSFER_DIRECTION_MASTER_WRITE;
			if (req->rx_data == NULL) {
				I2C_Api_Recover(i2c);
				req->driver_status = EC_ERROR_INVAL;
				return;
			}
			if (slave_rx_remain != 0) {
				// Read out any data in the RX FIFO
				while ((slave_rx_remain > 0) && !(i2c->status & MXC_F_I2C_STATUS_RX_EMPTY)) {
					*(req->rx_data)++ = i2c->fifo;
					req->rx_num++;
					slave_rx_remain--;
				}
				// Set the RX threshold interrupt level
				if (slave_rx_remain >= (MXC_I2C_FIFO_DEPTH - 1)) {
					i2c->rx_ctrl0 = ((i2c->rx_ctrl0 & ~(MXC_F_I2C_RX_CTRL0_RX_THRESH)) |
													 (MXC_I2C_FIFO_DEPTH - 1) << MXC_F_I2C_RX_CTRL0_RX_THRESH_POS);
				} else {
					i2c->rx_ctrl0 = ((i2c->rx_ctrl0 & ~(MXC_F_I2C_RX_CTRL0_RX_THRESH)) |
													 (slave_rx_remain) << MXC_F_I2C_RX_CTRL0_RX_THRESH_POS);
				}
				// Enable RXTH interrupt and Error interrupts
				i2c->int_en0 |= (MXC_F_I2C_INT_EN0_RX_THRESH | I2C_ERROR);
				//i2c->int_en0 = inten0;
				if (int0 & I2C_ERROR) {
					i2c->int_en0 = 0;
					// Calculate the number of bytes sent by the slave
					req->tx_num = states[i2c_num].num_wr - ((i2c->tx_ctrl1 & MXC_F_I2C_TX_CTRL1_TX_FIFO) >> MXC_F_I2C_TX_CTRL1_TX_FIFO_POS);

					if (!req->sw_autoflush_disable) {
						// Manually clear the TXFIFO
						i2c->tx_ctrl0 |= MXC_F_I2C_TX_CTRL0_TX_FLUSH;
					}
					states[i2c_num].num_wr = 0;
					I2C_Api_Recover(i2c);
					req->driver_status = EC_ERROR_UNKNOWN;
					return;
				}
			} else {
				// Disable RXTH interrupt
				i2c->int_en0 &= ~(MXC_F_I2C_INT_EN0_RX_THRESH);
				// Flush any extra bytes in the RXFIFO
				i2c->rx_ctrl0 |= MXC_F_I2C_RX_CTRL0_RX_FLUSH;
				// Store the current state of the slave
				states[i2c_num].slave_state = I2C_SLAVE_READ_COMPLETE;
			}
		}
	}
}

/* ************************************************************************** */
void I2C_Api_Handler(mxc_i2c_regs_t *i2c)
{
	if (i2c->ctrl & MXC_F_I2C_CTRL_MST && i2c->int_fl0) {
		// Service master interrupts if we're in master mode
		I2C_Api_MasterHandler(i2c);
	} else if (i2c->int_fl0 || i2c->int_fl1) {
		// Service the slave interrupts
		//I2C_SlaveHandler(i2c);
		I2C_Api_SlaveHandler(i2c);
	}
}

/* ************************************************************************** */
void I2C_Api_DrainTX(mxc_i2c_regs_t *i2c)
{
	i2c->tx_ctrl0 |= MXC_F_I2C_TX_CTRL0_TX_FLUSH;
}

/* ************************************************************************* */
static void I2C_Api_Recover(mxc_i2c_regs_t *i2c)
{
	// Disable and clear interrupts
	i2c->int_en0 = 0;
	i2c->int_en1 = 0;
	i2c->int_fl0 = i2c->int_fl0;
	i2c->int_fl1 = i2c->int_fl1;
	i2c->ctrl = 0;
	i2c->ctrl = MXC_F_I2C_CTRL_I2C_EN;
}
