/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * PI3USB9201 USB BC 1.2 Charger Detector driver.
 *
 * NOTE: The driver assumes that CHG_AL_N and SW_OPEN are not connected,
 * therefore the value of CHG_DET indicates whether the source is NOT a
 * low-power standard downstream port (SDP).  In order to use higher currents,
 * the system will have to charge ramp.
 */

#include "pi3usb9201.h"
#include "charge_manager.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "power.h"
#include "task.h"
#include "tcpm.h"
#include "timer.h"
#include "usb_charge.h"
#include "usb_pd.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

static inline int raw_read8(int port, int offset, int *value)
{
	return i2c_read8(bc12_chips[port].i2c_port, bc12_chips[port].i2c_addr,
			 offset, value);
}

static inline int raw_write8(int port, int offset, int value)
{
	return i2c_write8(bc12_chips[port].i2c_port, bc12_chips[port].i2c_addr,
			  offset, value);
}

static void pi3usb9201_reg_dump(int port)
{
	int i;
	int reg;

	for (i = PI3USB9201_REG_CTRL_1; i < PI3USB9201_REG_HOST_STS; i++) {
		if (raw_read8(port, i, &reg) == EC_SUCCESS)
			CPRINTS("pi3usb9201: reg 0x%02x\t%x", i, reg);
	}
	cflush();
}

static int pi3usb9201_get_mode(int port, int *mode)
{
	int rv;

	rv = raw_read8(port, PI3USB9201_REG_CTRL_1, mode);
	if (rv)
		return rv;

	*mode = (*mode >> PI3USB9201_REG_CTRL_1_INT_MODE_BIT_SHIFT) &
		PI3USB9201_REG_CTRL_1_INT_MODE_MASK;

	return rv;
}

static int pi3usb9201_set_mode(int port, int desired_mode)
{
	int rv;
	int reg;

	rv = raw_read8(port, PI3USB9201_REG_CTRL_1, &reg);
	if (rv)
		return rv;

	reg &= ~(PI3USB9201_REG_CTRL_1_INT_MODE_MASK <<
		 PI3USB9201_REG_CTRL_1_INT_MODE_BIT_SHIFT);

	reg |= (desired_mode << PI3USB9201_REG_CTRL_1_INT_MODE_BIT_SHIFT);

	return  raw_write8(port, PI3USB9201_REG_CTRL_1, reg);
}

static void bc12_detect(int port)
{
	int mode;

	pi3usb9201_reg_dump(port);
	pi3usb9201_get_mode(port, &mode);
	CPRINTS("bc12: mode = %x", mode);
}

void usb_charger_set_switches(int port, enum usb_switch setting)
{
	int mode;


	/* /\* If switch is not changing then return *\/ */
	/* if (setting == usb_switch_state[port]) */
	/* 	return; */

	/* mutex_lock(&usb_switch_lock[port]); */
	/* if (setting != USB_SWITCH_RESTORE) */
	/* 	usb_switch_state[port] = setting; */

	/* pi3usb9281_set_switches(port, usb_switch_state[port]); */

	/* mutex_unlock(&usb_switch_lock[port]); */

	CPRINTS("bc12: set switch %d", setting);

	if (setting == USB_SWITCH_DISCONNECT) {
		mode = PI3USB9201_POWER_DOWN;
	} else if (setting == USB_SWITCH_CONNECT) {
		mode = PI3USB9201_USB_PATH_ON;
	} else {
		mode = PI3USB9201_USB_PATH_ON;
	}

	pi3usb9201_set_mode(port, mode);
}


void usb_charger_task(void *u)
{
	int port = (task_get_current() == TASK_ID_USB_CHG_P0 ? 0 : 1);
	//int interrupt;
	uint32_t evt;

	/* Initialize chip and enable interrupts */
	//pi3usb9201_init(port);

	bc12_detect(port);

	while (1) {
		/* Wait for interrupt */
		evt = task_wait_event(-1);

		/* Interrupt from the Pericom chip, determine charger type */
		if (evt & USB_CHG_EVENT_BC12) {
			/* Read interrupt register to clear on chip */
			//pi3usb9281_get_interrupts(port);
			bc12_detect(port);
		}
		/* } else if (evt & USB_CHG_EVENT_INTR) { */
		/* 	/\* Check the interrupt register, and clear on chip *\/ */
		/* 	interrupt = pi3usb9281_get_interrupts(port); */
		/* 	if (interrupt & attach_mask) */
		/* 		bc12_detect(port); */
		/* } */

		/*
		 * Re-enable interrupts on pericom charger detector since the
		 * chip may periodically reset itself, and come back up with
		 * registers in default state. TODO(crosbug.com/p/33823): Fix
		 * these unwanted resets.
		 */
		if (evt & USB_CHG_EVENT_VBUS) {
			//pi3usb9281_enable_interrupts(port);
#ifndef CONFIG_USB_PD_VBUS_DETECT_TCPC
			CPRINTS("VBUS p%d %d", port,
				pd_snk_is_vbus_provided(port));
#endif
		}
	}
}
