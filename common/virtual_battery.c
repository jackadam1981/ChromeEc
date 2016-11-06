/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Virtual battery cross-platform code for Chrome EC */

#include "battery.h"
#include "charge_state.h"
#include "i2c.h"
#include "system.h"
#include "util.h"
#include "virtual_battery.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_I2C, outstr)
#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)

/*
 * The state machine used to parse smart battery command
 * to support virtual battery
 */
enum batt_cmd_parse_state {
	IDLE = 0,
	START = 1,
	WRITE_VB,
	READ_VB,
};

static enum batt_cmd_parse_state sb_cmd_state;
static uint8_t cache_hit;
static const uint8_t *batt_cmd_head;
static int acc_write_len;

int virtual_battery_handler(struct ec_response_i2c_passthru *resp,
				   int in_len, int *rv, int xferflags,
				   int read_len, int write_len,
				   const uint8_t *out)
{

#if defined(CONFIG_BATTERY_PRESENT_GPIO) || \
	defined(CONFIG_BATTERY_PRESENT_CUSTOM)
	/*
	 * If the battery isn't present, return a NAK (which we
	 * would have gotten anyways had we attempted to talk to
	 * the battery.)
	 */
	if (battery_is_present() != BP_YES) {
		resp->i2c_status = EC_I2C_STATUS_NAK;
		return EC_ERROR_INVAL;
	}
#endif
	switch (sb_cmd_state) {
	case IDLE:
		/*
		 * A legal battery command must start
		 * with a i2c write for reg index.
		 */
		if (write_len == 0) {
			resp->i2c_status = EC_I2C_STATUS_NAK;
			break;
		}
		/* Record the head of battery command. */
		batt_cmd_head = out;
		sb_cmd_state = START;
		*rv = 0;
		break;
	case START:
		if (write_len > 0) {
			sb_cmd_state = WRITE_VB;
			*rv = 0;
		} else {
			sb_cmd_state = READ_VB;
			/* Test if the reg is cached. */
			*rv = virtual_battery_operation(batt_cmd_head,
						&resp->data[in_len], 0, 0);
			/*
			 * If the reg is not cached in the virtual memory,
			 * we need to physically write the reg index to
			 * the battry.
			 */
			if (*rv) {
				*rv = i2c_xfer(
					I2C_PORT_VIRTUAL_BATTERY,
					VIRTUAL_BATTERY_ADDR,
					batt_cmd_head,
					1,
					&resp->data[in_len],
					0,
					I2C_XFER_START);
				/* sent a stop bit here */
				if (*rv) {
					if (*rv == EC_ERROR_TIMEOUT) {
						resp->i2c_status =
						EC_I2C_STATUS_TIMEOUT;
					} else {
						resp->i2c_status =
						EC_I2C_STATUS_NAK;
					}
					return EC_ERROR_INVAL;
				}
				*rv = 1;
			} else
				cache_hit = 1;
		}
		break;
	case WRITE_VB:
		if (write_len == 0) {
			resp->i2c_status = EC_I2C_STATUS_NAK;
			return EC_ERROR_INVAL;
		}
		break;
	case READ_VB:
		if (read_len == 0) {
			resp->i2c_status = EC_I2C_STATUS_NAK;
			return EC_ERROR_INVAL;
		}
		/*
		 * Do not send the command to battery
		 * if the reg is cached.
		 */
		if (cache_hit)
			*rv = 0;
		break;
	default:
		return EC_ERROR_INVAL;
	}
	acc_write_len += write_len;
	/* the last message */
	if (xferflags & I2C_XFER_STOP) {
		/* check if the reg index is cached */
		if (sb_cmd_state == WRITE_VB || sb_cmd_state == START) {
			virtual_battery_operation(batt_cmd_head,
						&resp->data[in_len],
						0,
						acc_write_len);
		} else {
			if (cache_hit) {
				virtual_battery_operation(batt_cmd_head,
							&resp->data[0],
							in_len + read_len,
							0);
			}
		}
		/* Reset the state in the end of message */
		sb_cmd_state = IDLE;
		cache_hit = 0;
		acc_write_len = 0;
	}
	return EC_RES_SUCCESS;
}

int virtual_battery_operation(const uint8_t *batt_cmd_head,
			      uint8_t *dest,
			      int read_len,
			      int write_len)
{
	int val;
	static int batt_mode_cache;
	const struct batt_params *curr_batt;

	curr_batt = charger_current_battery_params();
	switch (*batt_cmd_head) {
	case SB_BATTERY_MODE:
		if (write_len == 3) {
			batt_mode_cache = batt_cmd_head[1] |
					  (batt_cmd_head[2] << 8);
		} else if (read_len > 0) {
			if (batt_mode_cache == 0) {
				i2c_xfer(I2C_PORT_VIRTUAL_BATTERY,
					VIRTUAL_BATTERY_ADDR,
					batt_cmd_head,
					1,
					(uint8_t *)&batt_mode_cache,
					read_len,
					I2C_XFER_SINGLE);
			}
			memcpy(dest, &batt_mode_cache, read_len);
		}
		break;
	case SB_SERIAL_NUMBER:
		val = strtoi(host_get_memmap(EC_MEMMAP_BATT_SERIAL), NULL, 16);
		memcpy(dest, &val, read_len);
		break;
	case SB_VOLTAGE:
		memcpy(dest, &(curr_batt->voltage), read_len);
		break;
	case SB_RELATIVE_STATE_OF_CHARGE:
		memcpy(dest, &(curr_batt->state_of_charge), read_len);
		break;
	case SB_TEMPERATURE:
		memcpy(dest, &(curr_batt->temperature), read_len);
		break;
	case SB_CURRENT:
		memcpy(dest, &(curr_batt->current), read_len);
		break;
	case SB_FULL_CHARGE_CAPACITY:
		val = curr_batt->full_capacity;
		if (batt_mode_cache & MODE_CAPACITY)
			val = val * curr_batt->voltage / 10;
		memcpy(dest, &val, read_len);
		break;
	case SB_BATTERY_STATUS:
		memcpy(dest, &(curr_batt->status), read_len);
		break;
	case SB_CYCLE_COUNT:
		memcpy(dest, (int *)host_get_memmap(EC_MEMMAP_BATT_CCNT),
			read_len);
		break;
	case SB_DESIGN_CAPACITY:
		val = *(int *)host_get_memmap(EC_MEMMAP_BATT_DCAP);
		if (batt_mode_cache & MODE_CAPACITY)
			val = val * curr_batt->voltage / 10;
		memcpy(dest, &val, read_len);
		break;
	case SB_DESIGN_VOLTAGE:
		memcpy(dest, (int *)host_get_memmap(EC_MEMMAP_BATT_DVLT),
		       read_len);
		break;
	case SB_REMAINING_CAPACITY:
		val = curr_batt->remaining_capacity;
		if (batt_mode_cache & MODE_CAPACITY)
			val = val * curr_batt->voltage / 10;
		memcpy(dest, &val, read_len);
		break;
	default:
		return EC_ERROR_INVAL;
	}
	return EC_SUCCESS;
}
