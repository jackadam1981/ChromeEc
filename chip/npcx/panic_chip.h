/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_PANIC_CHIP_H
#define __CROS_EC_PANIC_CHIP_H

/* Save panic data to BBRAM */
void panic_data_backup(void);

/* Restore panic data from BBRAM */
void panic_data_restore(void);

#endif /* __CROS_EC_PANIC_CHIP_H */
