/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "console.h"
#include "gpio.h"
#include "panic.h"
#include "pwm.h"
#include "task.h"

/* Keep track of available charge for each charge task */
static int available_charge_ma[NUM_CHARGE_PORTS][NUM_CHARGE_SUPPLIERS];

/* Mutex for accessing available_charge_ma */
static struct mutex available_charge_mutex;

/**
 * Update available charge for a given port / supplier.
 *
 * @param port		Charge port to update.
 * @param supplier	Charge supplier to update.
 * @param charge_ma	Updated charge (mA).
 */
void update_available_charge(enum charge_port port,
			     enum charge_supplier supplier,
			     int charge_ma)
{
	int wake_task = 0;

	/* Lock charge table and update if needed. */
	mutex_lock(&available_charge_mutex);
	if (available_charge_ma[port][supplier] != charge_ma) {
		available_charge_ma[port][supplier] = charge_ma;
		wake_task = 1;
	}
	mutex_unlock(&available_charge_mutex);

	/* If we made a change, wake the charge manager. */
	if (wake_task)
		task_wake(TASK_ID_CHARGE_MANAGER);
}

/**
 * Set active charge port -- only one port can be active at a time.
 *
 * @param port		Charge port to enable.
 */
static void set_active_charge_port(enum charge_port port)
{
	int usb_c0_en, usb_c1_en;

	switch (port) {
	case CHARGE_PORT_0:
		usb_c0_en = 1;
		usb_c1_en = 0;
		break;
	case CHARGE_PORT_1:
		usb_c0_en = 0;
		usb_c1_en = 1;
		break;
	default:
		usb_c0_en = usb_c1_en = 0;
		break;
	}

	gpio_set_level(GPIO_USB_C0_CHARGE_EN_L, !usb_c0_en);
	gpio_set_level(GPIO_USB_C1_CHARGE_EN_L, !usb_c1_en);
	ccprintf("Set active charge port %d\n", port);
}

/**
 * Set the charge limit based upon desired maximum.
 *
 * @param charge_ma	Desired charge limit (mA).
 */
static void set_charge_limit(int charge_ma)
{
	int pwm_duty = MA_TO_PWM(charge_ma);
	if (pwm_duty < 0)
		pwm_duty = 0;
	else if (pwm_duty > 100)
		pwm_duty = 100;

	pwm_set_duty(PWM_CH_ILIM, pwm_duty);
	ccprintf("Set ilim duty %d\n", pwm_duty);
}

/**
 * Charge manager task -- task responsible for selecting the active charge
 * port and charge current limit.
 */
void charge_manager_task(void)
{
	enum charge_port port = CHARGE_PORT_NONE;
	int charge_ma = 0;

	/* Initialize to not charging */
	set_charge_limit(charge_ma);
	set_active_charge_port(port);

	while (1) {
		enum charge_port new_port;
		enum charge_supplier new_supplier;
		int new_charge_ma;
		int i;

		/* Task will be woken on available_charge_ma change */
		task_wait_event(-1);

		/*
		 * Charge supplier selection logic:
		 * 1. Prefer PD over BC1.2
		 * 2. Prefer higher current over lower
		 */
		new_supplier = CHARGE_SUPPLIER_BC12;
		mutex_lock(&available_charge_mutex);

		/* Check for a PD port we can charge from */
		for (i = 0; i < NUM_CHARGE_PORTS; ++i) {
			if (available_charge_ma[i][CHARGE_SUPPLIER_PD] > 0) {
				new_supplier = CHARGE_SUPPLIER_PD;
				break;
			}
		}

		/* Check for the port which can supply the maximum current */
		new_port = CHARGE_PORT_NONE;
		for (i = 0; i < NUM_CHARGE_PORTS; ++i) {
			if (available_charge_ma[i][new_supplier] > 0)
				if (new_port == CHARGE_PORT_NONE ||
				    available_charge_ma[i][new_supplier] >
				    available_charge_ma[new_port][new_supplier])
					new_port = i;
		}

		/* Update chosen port + available charge */
		if (new_port == CHARGE_PORT_NONE)
			new_supplier = CHARGE_SUPPLIER_NONE;

		if (new_supplier == CHARGE_SUPPLIER_NONE)
			new_charge_ma = 0;
		else
			new_charge_ma = available_charge_ma[new_port]
							   [new_supplier];

		mutex_unlock(&available_charge_mutex);

		/* Change the chage limit + charge port if changed */
		if (new_port != port || new_charge_ma != charge_ma) {
			set_charge_limit(new_charge_ma);
			set_active_charge_port(new_port);

			charge_ma = new_charge_ma;
			port = new_port;
		}
	}
}

