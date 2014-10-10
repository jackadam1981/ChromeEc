/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CHARGE_MANAGER_H
#define __CHARGE_MANAGER_H

/* Charge port that indicates no active port */
#define CHARGE_PORT_NONE -1
/* Initial charge current state */
#define CHARGE_CURRENT_UNINITIALIZED -1

/* We have separate tasks for PD charging + BC1.2 charging */
enum charge_supplier {
	CHARGE_SUPPLIER_NONE = -1,
	CHARGE_SUPPLIER_PD = 0,
	CHARGE_SUPPLIER_BC12 = 1,
	CHARGE_SUPPLIER_COUNT
};

/* Called by charging tasks to update their available charge */
void charge_manager_update(int port,
			   enum charge_supplier supplier,
			   int charge_ma);

#endif /* __CHARGE_MANAGER_H */
