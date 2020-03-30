/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* INTC control module */

#ifndef __CROS_EC_INTC_H
#define __CROS_EC_INTC_H

int chip_get_ec_int(void);
int chip_get_intc_group(int irq);

#endif /* __CROS_EC_INTC_H */
