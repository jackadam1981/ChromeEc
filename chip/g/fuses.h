/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_FUSES_H
#define __CROS_FUSES_H

#define FUSE_ENABLED 5

/* Returns 1 if the fuses are locked */
int fuses_are_locked(void);

/* Initialize all fuse_prog registers to values currently in the fuses. */
void init_fuses(void);

/* Start the fuse override and wait until the operation is complente.i */
void fuse_override_start(void);
#endif  /* __CROS_FUSES_H */
