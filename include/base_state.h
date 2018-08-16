/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Return 1 if base attached, 0 otherwise.
 */
int base_get_state(void);
void base_set_state(int mode);
