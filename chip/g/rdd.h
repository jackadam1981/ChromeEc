/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_RDD_H
#define __CROS_RDD_H

/**
 * Initialize RDD module
 */
void init_rdd_state(void);

/**
 * Print debug accessory detect state
 *
 * @param with_keepalive_status:
 *        true if it should print rddkeepalive true status over rdd status.
 *        false if it should print rdd status only.
 */
void print_rdd_state(bool with_keepalive_status);

/**
 * Get instantaneous cable detect state
 *
 * @return 1 if debug accessory is detected, 0 if not detected.
 */
uint8_t rdd_is_detected(void);


/**
 * Get rddkeepalive status
 *
 * @return true if rdd is kept to be alive, or false otherwise.
 */
bool rdd_is_keepalive(void);

#endif  /* __CROS_RDD_H */
