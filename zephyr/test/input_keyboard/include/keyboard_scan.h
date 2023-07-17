/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

enum kb_scan_disable_masks {
	KB_SCAN_DISABLE_A = 1 << 0,
	KB_SCAN_DISABLE_B = 1 << 1,
};

void keyboard_scan_enable(int enable, enum kb_scan_disable_masks mask);
