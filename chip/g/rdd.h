/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_RDD_H
#define __CROS_RDD_H

/* Current state of debug cable */
int debug_cable_is_attached(void);

/* Board-specific callbacks below */

/* Called when detached from debug cable */
void rdd_detached(void);

/* Called when attached to debug cable */
void rdd_attached(void);

/* Called during initialization */
void rdd_setup(void);

#endif  /* __CROS_RDD_H */
