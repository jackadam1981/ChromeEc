/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Rotor Crystal driver. */

#include "common.h"
#include "console.h"
#include "driver/tcpm/tcpci.h"
#include "driver/tcpm/tcpm.h"
#include "ec_commands.h"
#include "host_command.h"
#include "i2c.h"
#include "ipc.h"
#include "registers.h"
#include "usb_mux.h"
#include "usb_pd_tcpm.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_SYSTEM, outstr)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

/* ----- I2C helper functions ----- */
/**
 * Write a value to a register on the Crystal IC.
 *
 * @param chip	Which Crystal chip.
 * @param reg	Crystal register address.
 * @param val	Value to write.
 *
 * @return EC_SUCCESS on success, otherwise an error.
 */
static int crystal_write(uint8_t chip, uint16_t reg, uint32_t val)
{
	int slave_addr;
	int rv;
	int i;
	uint8_t buf[sizeof(reg) + sizeof(val)];

	if (chip < CONFIG_USB_PD_PORT_COUNT)
		slave_addr = tcpc_config[chip].i2c_slave_addr;
	else
		return EC_ERROR_INVAL;

	/* First, write the register address. */
	buf[0] = reg & 0xFF;
	buf[1] = (reg >> 8) & 0xFF;
	/* Then, the value to be written. */
	for (i = 2; i < sizeof(buf); i++)
		buf[i] = (val >> (8*(i - 2))) & 0xFF;

	i2c_lock(I2C_PORT_TCPC, 1);
	rv = i2c_xfer(I2C_PORT_TCPC, slave_addr, buf, sizeof(buf), NULL, 0,
		      I2C_XFER_SINGLE);
	i2c_lock(I2C_PORT_TCPC, 0);

	return rv;
}

/**
 * Read a value from a register on the Crystal IC.
 *
 * @param chip		Which Crystal chip.
 * @param reg		Crystal register address.
 * @param in		Pointer to buffer to place read values.
 * @param in_size	How many bytes to read.
 *
 * @return EC_SUCCESS on success, otherwise an error.
 */
static int crystal_read(uint8_t chip, uint16_t reg, uint8_t *in, int in_size)
{
	int slave_addr;
	int rv;
	uint8_t buf[sizeof(reg)];

	if (chip < CONFIG_USB_PD_PORT_COUNT)
		slave_addr = tcpc_config[chip].i2c_slave_addr;
	else
		return EC_ERROR_INVAL;

	/* Write the register address. */
	buf[0] = reg & 0xFF;
	buf[1] = (reg >> 8) & 0xFF;

	i2c_lock(I2C_PORT_TCPC, 1);
	rv = i2c_xfer(I2C_PORT_TCPC, slave_addr, buf, sizeof(buf), NULL, 0,
		      I2C_XFER_START);
	if (rv != EC_SUCCESS) {
		i2c_lock(I2C_PORT_TCPC, 0);
		return rv;
	}
	rv = i2c_xfer(I2C_PORT_TCPC, slave_addr, NULL, 0, in, in_size,
		      I2C_XFER_SINGLE);
	i2c_lock(I2C_PORT_TCPC, 0);

	return rv;
}

int crystal_check_usb_wake_evt(uint8_t chip)
{
	int rv, i, csum;
	uint32_t val;
	uint16_t reg;
	uint32_t *buf;
	struct ec_host_request *r;
	struct ec_params_crystal_usb_wake_evt params;
	uint8_t *p, *out;

	/* Check USB_DPMU_INT_STAT register. */
	reg = CRYSTAL_USB_DPMU_INT_STAT;
	rv = crystal_read(chip, reg, (uint8_t *)&val, sizeof(val));
	if (rv != EC_SUCCESS)
		return rv;

	if (val & 0x1) {
		/* A USB wake event has occurred. */
		/*
		 * Mask USB-DPMU interrupts.  This is necessary to clear the
		 * interrupt after the USB event interrupt has occurred .
		 */
		val = 0;
		reg = CRYSTAL_USB_DPMU_INT_CTRL;
		rv = crystal_read(chip, reg, (uint8_t *)&val, sizeof(val));
		if (rv != EC_SUCCESS)
			return rv;

		val |= 1;
		rv = crystal_write(chip, reg, val);
		if (rv != EC_SUCCESS)
			return rv;

		/* Send a Crystal USB wake event message to the APMU. */
		buf = get_ipc_buffer(ROTOR_MCU_APMU_IPC);
		r = (struct ec_host_request *)buf;
		memset(buf, 0, ROTOR_MCU_IPC_BUF_LEN);

		/* Fill out the host command request. */
		r->struct_version = 3;
		r->checksum = 0;
		r->command = EC_CMD_CRYSTAL_USB_WAKE_EVT;
		r->command_version = 0;
		r->data_len = sizeof(params);

		p = (uint8_t *)buf + sizeof(struct ec_host_request);
		params.chip = chip;
		memcpy(p, &params, sizeof(params));

		/* Compute checksum. */
		csum = 0;
		out = (uint8_t *)buf;
		for (i = (sizeof(struct ec_host_request) +
			  sizeof(struct ec_params_crystal_usb_wake_evt));
		     i > 0;
		     i--)
			csum += *out++;

		/* Fill in checksum now. */
		r->checksum = (uint8_t)(-csum);

		/* Now that the message is ready, send it to the APMU. */
		send_message(ROTOR_MCU_APMU_IPC, buf, 0);
	}

	return EC_SUCCESS;
}

/* ----- Crystal Power State Management ----- */
/* Register field values for current power state. */
#define CRYSTAL_STATE_HIBERNATE 1
#define CRYSTAL_STATE_NORMAL 2
/**
 * Change the current power state of a Crystal chip.
 *
 * @param chip		Which Crystal to change.
 * @param desired_state	The new state to change to.
 * @param new_state	A pointer to the new current power state.
 *
 * @return EC_SUCCESS when able to change the power state, otherwise an error.
 */
static int change_power_state(uint8_t chip,
			      enum crystal_power_state desired_state,
			      uint8_t *new_state)
{
	uint32_t rv;
	uint32_t val;
	int reg;
	int retries;

	if ((desired_state != PS_NORMAL) && (desired_state != PS_HIBERNATE))
		return EC_ERROR_INVAL;

	/* Read CRYSTAL_PMU0_PWRST_CTRL. */
	reg = CRYSTAL_PMU0_PWRST_CTRL;
	rv = crystal_read(chip, reg, (uint8_t *)&val, sizeof(val));
	if (rv != EC_SUCCESS)
		return rv;

	/* Clear reserved bits.  Reserved bits must be written as 0. */
	val &= ~((0xFFE << 16) | (0x7E << 8) | 0xFE);

	switch (desired_state) {
	case PS_NORMAL:
		val |= (1 << 0);
		break;
	case PS_HIBERNATE:
		val |= (1 << 8);
		break;
	default:
		break;
	}

	/* Write back to PMU0_PWRST_CTRL. */
	rv = crystal_write(chip, reg, (int)val);
	if (rv != EC_SUCCESS)
		return rv;

	/*
	 * Check to see state changed by reading PMU_PWR_PSTATE.  Bit 0 will be
	 * set to 1 when the power command is complete.
	 */
	reg = CRYSTAL_PMU0_PWR_STATE;
	retries = 3;
	do {
		rv = crystal_read(chip, reg, (uint8_t *)&val, sizeof(val));
		if (rv != EC_SUCCESS)
			return rv;

		/* Try again if it's taking too long. */
		retries--;
		if (retries <= 0) {
#ifdef CONFIG_BRINGUP
			CPUTS("Took too long to change Crystal power state!");
#endif /* defined(CONFIG_BRINGUP) */
			break;
		}
	} while (!(val & 1));

	/* Return whatever state is indicated. */
	*new_state = (uint8_t)((val & 0x70) >> 4);

	return EC_SUCCESS;
}

static int host_command_power_state_set(struct host_cmd_handler_args *args)
{
	const struct ec_params_crystal_ps_set *p = args->params;
	struct ec_response_crystal_ps_set *r = args->response;
	int rv;

	args->response_size = sizeof(*r);
	memset(r, 0, sizeof(*r));
	r->chip = p->chip;
	rv = change_power_state(p->chip, p->requested_state, &r->new_state);

	/* Translate error code to host command error code. */
	if (rv == EC_ERROR_INVAL)
		rv = EC_RES_INVALID_PARAM;
	else if (rv == EC_ERROR_TIMEOUT)
		rv = EC_RES_TIMEOUT;
	else if (rv == EC_SUCCESS)
		rv = EC_RES_SUCCESS;
	else
		rv = EC_RES_ERROR;

	return rv;
}
DECLARE_HOST_COMMAND(EC_CMD_CRYSTAL_POWER_STATE_SET,
		     host_command_power_state_set, EC_VER_MASK(0));

static int command_power_state_set(int argc, char **argv)
{
	uint32_t val;
	int chip;
	int rv;

	if (argc == 2) {
		/* Just get the current power state. */
		chip = atoi(argv[1]);
		rv = crystal_read(chip, CRYSTAL_PMU0_PWR_STATE, (uint8_t *)&val,
				  sizeof(val));
		if (rv != EC_SUCCESS)
			return rv;

		/* Return the current reported power state. */
		switch ((val & 0x70) >> 4) {
		case CRYSTAL_STATE_HIBERNATE:
			ccprintf("Crystal %d cur power state: Hibernate\n",
				 chip);
			break;
		case CRYSTAL_STATE_NORMAL:
			ccprintf("Crystal %d cur power state: Normal\n", chip);
			break;
		default:
			/* Just returning whatever else is there. */
			ccprintf("Crystal %d cur power state: %d\n", chip,
				 ((val & 0x70) >> 4));
			break;
		}

	} else if (argc == 3) {
		/* Set the power state. */
		chip = atoi(argv[1]);
		rv = change_power_state((uint8_t)chip, atoi(argv[2]),
					(uint8_t *)&val);
		if (rv == EC_SUCCESS) {
			switch ((uint8_t)val) {
			case CRYSTAL_STATE_HIBERNATE:
				ccprintf("Crystal %d changed power state"
					 " to Hibernate.\n", chip);
				break;

			case CRYSTAL_STATE_NORMAL:
				ccprintf("Crystal %d changed power state"
					 " to Normal.\n", chip);
				break;

			default:
				/* Just returning whatever else is there. */
				ccprintf("Crystal %d cur power state: %d\n",
					 chip, (uint8_t)val);
				break;
			}
		}
		return rv;

	} else {
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;

}
DECLARE_CONSOLE_COMMAND(crystalps, command_power_state_set,
			"chip<0,1> [new ps<0,1>]", NULL, NULL);
