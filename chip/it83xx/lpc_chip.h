/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* KBC/PMC control module for IT83xx. */

#ifndef __CROS_EC_IT83XX_LPC_H
#define __CROS_EC_IT83XX_LPC_H

void lpc_kbc_ibf_interrupt(void);
void lpc_kbc_obe_interrupt(void);

#endif /* __CROS_EC_IT83XX_LPC_H */
