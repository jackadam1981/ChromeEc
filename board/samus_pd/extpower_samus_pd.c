/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB charging control for samus_pd board */

#include "console.h"
#include "pi3usb9281.h"

/**
 * Returns the current limit according to device type / charger type.
 *
 * @param pi_type	Type register, as read from Pericom.
 * @param pi_charge	Charge register, as read from Pericom.
 */
static int usb_get_ilim(int pi_type, int pi_charge)
{
	/* Limit USB port current. 500mA for not listed types. */
	int current_limit_ma = 500;

	if (pi_charge & PI3USB9281_CHG_CAR_TYPE1 ||
	    pi_charge & PI3USB9281_CHG_CAR_TYPE2)
		current_limit_ma = 3000;
	else if (pi_charge & PI3USB9281_CHG_APPLE_1A)
		current_limit_ma = 1000;
	else if (pi_charge & PI3USB9281_CHG_APPLE_2A)
		current_limit_ma = 2000;
	else if (pi_charge & PI3USB9281_CHG_APPLE_2_4A)
		current_limit_ma = 2400;
	else if (pi_type & PI3USB9281_TYPE_CDP)
		current_limit_ma = 1500;
	else if (pi_type & PI3USB9281_TYPE_DCP)
		current_limit_ma = 1500;

	return current_limit_ma;
}

/*
 * Update available charge. Called from deferred task, queued on Pericom
 * interrupt.
 */
void usb_charger_update(int port)
{
	/* Check interrupt register */
	int interrupts = pi3usb9281_get_interrupts(port);

	/* Attachment: decode + update available charge */
	if (interrupts & PI3USB9281_INT_ATTACH) {
		int lim = usb_get_ilim(
			pi3usb9281_get_device_type(port),
			pi3usb9281_get_charger_status(port));
		update_available_charge(port, CHARGE_SUPPLIER_BC12, lim);
	/* Detachment: update available charge to 0 */
	} else if (interrupts & PI3USB9281_INT_DETACH)
		update_available_charge(port, CHARGE_SUPPLIER_BC12, 0);
}
