/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* AOZ1380 Type-C Power Path Controller */

#ifndef __CROS_EC_AOZ1380_H
#define __CROS_EC_AOZ1380_H

struct ppc_drv;
extern const struct ppc_drv aoz1380_drv;

/**
 * Interrupt Handler for the AOZ1380.
 *
 * @param port: The Type-C port which triggered the interrupt.
 */
void aoz1380_interrupt(int port);

#endif /* defined(__CROS_EC_AOZ1380_H) */
