/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Fingerprint sensor interface */
#include <stdbool.h>

#ifndef __CROS_EC_FPSENSOR_MODES_H
#define __CROS_EC_FPSENSOR_MODES_H

#define FP_MODE_ANY_CAPTURE \
	(FP_MODE_CAPTURE | FP_MODE_ENROLL_IMAGE | FP_MODE_MATCH)
#define FP_MODE_ANY_DETECT_FINGER \
	(FP_MODE_FINGER_DOWN | FP_MODE_FINGER_UP | FP_MODE_ANY_CAPTURE)
#define FP_MODE_ANY_WAIT_IRQ (FP_MODE_FINGER_DOWN | FP_MODE_ANY_CAPTURE)

bool fp_match_success(int match_result);

#endif /* __CROS_EC_FPSENSOR_MODES_H */
