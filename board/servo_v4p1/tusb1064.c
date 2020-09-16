/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "i2c.h"
#include "tusb1064.h"
#include "ioexpanders.h"
#include "usb_mux.h"

int init_tusb1064(int port)
{
	int val, reg;


	switch(board_id_cached()){

	case BOARD_ID_REV0:
		//REV0
		/* Enable the TUSB1064 redriver */
		// Actually a NOOP since I2C_EN is PP3300 on REV0
		cmux_en(1);
		break;
	case BOARD_ID_REV1:
		//REV1
		vbus_dischrg_en(0);
		break;
	default:
		break;
	}

	// Default to "Floating" DP Equalization
	reg=TUSB1064_DP1EQ(TUSB1064_DP_EQ_RX_10_0_DB) &
			TUSB1064_DP3EQ(TUSB1064_DP_EQ_RX_10_0_DB);
	val=tusb1064_write_byte(port, TUSB1064_REG_DP1DP3EQ_SEL, reg);
	if (val)
		return val;

	reg=TUSB1064_DP0EQ(TUSB1064_DP_EQ_RX_10_0_DB) &
			TUSB1064_DP2EQ(TUSB1064_DP_EQ_RX_10_0_DB);
	val=tusb1064_write_byte(port, TUSB1064_REG_DP0DP2EQ_SEL, reg);
	if (val)
		return val;

	/* Disconnect USB3.1 and DP */
	val=tusb1064_read_byte(port, TUSB1064_REG_GENERAL, &reg);
	if (val)
		return val;
	/* Read Modify Write (test) */
	reg &= ~REG_GENERAL_CTLSEL_MASK;
	reg |= REG_GENERAL_CTLSEL_DISABLE;
	val=tusb1064_write_byte(port, TUSB1064_REG_GENERAL, reg);
	if(val)
		return val;

	/* Disable AUX mux override */
	reg=((~TUSB1064_AUXDPCTRL_AUX_SNOOP_DISABLE & 0) | 
		 (~TUSB1064_AUXDPCTRL_AUX_SBU_OVR & 0) |
		 (~(TUSB1064_AUXDPCTRL_DP3_DISABLE |
		 	TUSB1064_AUXDPCTRL_DP2_DISABLE |
		 	TUSB1064_AUXDPCTRL_DP1_DISABLE |
		 	TUSB1064_AUXDPCTRL_DP0_DISABLE) & 0) );
	val=tusb1064_write_byte(port, TUSB1064_REG_AUXDPCTRL, reg);
	if(val)
		return val;

	return EC_SUCCESS;
}



// TODO: Get rid of these hardcoded functions
//--------------
int tusb1064_write_byte(int port, uint8_t reg, int val)
{
	return i2c_write8(port, TUSB1064_ADDR_FLAGS, reg, val);
}

int tusb1064_read_byte(int port, uint8_t reg, int *val)
{
	return i2c_read8(port, TUSB1064_ADDR_FLAGS, reg, val);
}
//--------------


// Proper generic functions
int tusb1064_init(const struct usb_mux *me)
{
	return init_tusb1064(me->i2c_port);
}


int tusb1064_read(const struct usb_mux *me, uint8_t reg, int *val)
{
	return i2c_read8(me->i2c_port, me->i2c_addr_flags,
			 reg, val);
}

int tusb1064_write(const struct usb_mux *me, uint8_t reg, int val)
{
	return i2c_write8(me->i2c_port, me->i2c_addr_flags,
			  reg, val);
}


/* Writes control register to set switch mode */
static int tusb1064_set_mux(const struct usb_mux *me, mux_state_t mux_state)
{
	/*
	 * For CE_DP, CE_USB, and FLIP, disable pin control and enable I2C
	 * control.
	 */

	int reg;
	ccprintf("tusb1064_set_mux [0x%X]\n",mux_state);

	reg =( (0 & REG_GENERAL_DP_EN_CTRL)
		| REG_GENERAL_CTLSEL_DISABLE );

	if (mux_state & USB_PD_MUX_USB_ENABLED)
		reg |= REG_GENERAL_CTLSEL_USB3;
	if (mux_state & USB_PD_MUX_DP_ENABLED)
		reg |= REG_GENERAL_CTLSEL_ANYDP;
	if (mux_state & USB_PD_MUX_POLARITY_INVERTED)
		reg |= REG_GENERAL_FLIPSEL;

	return tusb1064_write(me, TUSB1064_REG_GENERAL, reg);
}

/* Reads control register and updates mux_state accordingly */
static int tusb1064_get_mux(const struct usb_mux *me, mux_state_t *mux_state)
{
	int reg;
	int val;


	val = tusb1064_read(me, TUSB1064_REG_GENERAL, &reg);
	if(val)
		return EC_ERROR_INVAL;

	*mux_state = 0;
	if (reg & REG_GENERAL_CTLSEL_USB3)
		*mux_state |= USB_PD_MUX_USB_ENABLED;

	if (reg & REG_GENERAL_CTLSEL_ANYDP)
		*mux_state |= USB_PD_MUX_DP_ENABLED;

	if (reg & REG_GENERAL_FLIPSEL)
		*mux_state |= USB_PD_MUX_POLARITY_INVERTED;

	//TODO: Work on AUX Override bits too.
	ccprintf("tusb1064_get_mux [0x%X]\n", *mux_state);

	return EC_SUCCESS;
}

const struct usb_mux_driver tusb1064_usb_mux_driver = {
	.init = tusb1064_init,
	.set = tusb1064_set_mux,
	.get = tusb1064_get_mux,
};