/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Header file for Power Delivery chip CCG8 */

#ifndef __ZEPHYR_INCLUDE_DRIVERS_PDC_CCG8_H
#define __ZEPHYR_INCLUDE_DRIVERS_PDC_CCG8_H

/*
 * Retimer firmware update register: Send 0x01 command to go to Firmware update
 * mode for all the retimers(single/dual port PD) controlled by a CCG8 PD.
 */

#define PD_ICL_CTRL_REG 0x0040
#define PD_ICL_CTRL_REG_LEN 1

#define PD_MAX_READ_WRITE_SIZE 4

#endif /* __ZEPHYR_INCLUDE_DRIVERS_PDC_CCG8_H */
