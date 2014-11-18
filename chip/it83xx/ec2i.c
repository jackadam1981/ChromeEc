/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* EC2I control module for IT83xx. */

#include "hooks.h"
#include "ec2i_chip.h"
#include "registers.h"

static void ec2i_ec_access_enable(void)
{
	/*
	 * bit0: Host access to the PNPCFG registers is disabled.
	 * bit1: Host access to the BRAM registers is disabled.
	 */
	IT83XX_EC2I_LSIOHA |= 0x03;

	/* bit0: EC to I-Bus access enabled. */
	IT83XX_EC2I_IBCTL |= 0x01;

	/*
	 * Make sure that both CRIB and CWIB bits in IBCTL register
	 * are cleared.
	 * bit1: CRIB
	 * bit2: CWIB
	 */
	while (IT83XX_EC2I_IBCTL & 0x06)
		;

	/* Enable EC access to the PNPCFG registers */
	IT83XX_EC2I_IBMAE |= 0x01;
}

static void ec2i_ec_access_disable(void)
{
	/* Disable EC access to the PNPCFG registers. */
	IT83XX_EC2I_IBMAE &= ~0x01;

	/* Diable EC to I-Bus access. */
	IT83XX_EC2I_IBCTL &= ~0x01;

	/* Enable host access */
	IT83XX_EC2I_LSIOHA &= ~0x03;
}

/* EC2I write */
enum ec2i_message ec2i_write(enum host_pnpcfg_index index, uint8_t data)
{
	/* bit1 : VCC power on */
	if (IT83XX_SWUC_SWCTL1 & 0x02) {
		/* Enable EC2I EC access */
		ec2i_ec_access_enable();

		/* Set indirect host I/O offset. (index port) */
		IT83XX_EC2I_IHIOA = 0;
		IT83XX_EC2I_IHD = index;

		/* Read the CWIB bit in IBCTL until it returns 0. */
		while (IT83XX_EC2I_IBCTL & 0x04)
			;

		/* Set indirect host I/O offset. (data port) */
		IT83XX_EC2I_IHIOA = 1;
		IT83XX_EC2I_IHD = data;

		/* Read the CWIB bit in IBCTL until it returns 0. */
		while (IT83XX_EC2I_IBCTL & 0x04)
			;

		/* Disable EC2I EC access */
		ec2i_ec_access_disable();

		return EC2I_WIRTE_SUCCESS;
	} else {
		return EC2I_WIRTE_ERROR;
	}
}

/* EC2I read */
enum ec2i_message ec2i_read(enum host_pnpcfg_index index)
{
	uint8_t data;

	/* bit1 : VCC power on */
	if (IT83XX_SWUC_SWCTL1 & 0x02) {
		/* Enable EC2I EC access */
		ec2i_ec_access_enable();

		/* Set indirect host I/O offset. (index port) */
		IT83XX_EC2I_IHIOA = 0;
		IT83XX_EC2I_IHD = index;

		/* Read the CWIB bit in IBCTL until it returns 0. */
		while (IT83XX_EC2I_IBCTL & 0x04)
			;

		/* Set indirect host I/O offset. (data port) */
		IT83XX_EC2I_IHIOA = 1;

		/* This access is a read-action */
		IT83XX_EC2I_IBCTL |= 0x02;

		/* Read the CRIB bit in IBCTL until it returns 0. */
		while (IT83XX_EC2I_IBCTL & 0x02)
			;

		/* Read the data from IHD register */
		data = IT83XX_EC2I_IHD;

		/* Disable EC2I EC access */
		ec2i_ec_access_disable();

		return EC2I_READ_SUCCESS + data;
	} else {
		return EC2I_READ_ERROR;
	}
}
