/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CHARGE_MANAGER_H
#define __CHARGE_MANAGER_H

/* Charge current limit min / max, based on PWM duty cycle */
#define PWM_0_MA	500
#define PWM_100_MA	4000

/* Map current in milli-amps to PWM duty cycle percentage */
#define MA_TO_PWM(curr) (((curr) - PWM_0_MA) * 100 / (PWM_100_MA - PWM_0_MA))

/* samus_pd has two charge ports */
enum charge_port {
	CHARGE_PORT_NONE = -1,
	CHARGE_PORT_0 = 0,
	CHARGE_PORT_1 = 1,
	NUM_CHARGE_PORTS
};

/* We have separate tasks for PD charging + BC1.2 charging */
enum charge_supplier {
	CHARGE_SUPPLIER_NONE = -1,
	CHARGE_SUPPLIER_PD = 0,
	CHARGE_SUPPLIER_BC12 = 1,
	NUM_CHARGE_SUPPLIERS
};

/* Called by charging tasks to update their available charge */
void update_available_charge(enum charge_port port,
			     enum charge_supplier supplier,
			     int charge_ma);

#endif /* __CHARGE_MANAGER_H */
