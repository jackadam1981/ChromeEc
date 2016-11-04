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

/*
 * Run the operation to override the fuses with whatever is set in the
 * FUSE_PROG registers.
 */
void override_fuses(void);
#endif  /* __CROS_FUSES_H */
