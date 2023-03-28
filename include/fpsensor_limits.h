/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_FPSENSOR_LIMITS_H
#define __CROS_EC_FPSENSOR_LIMITS_H

/* These values are used to define the various buffer sizes and represent the
 * largest supported sizes. Each driver should verify that they do not require
 * more storage than is available based on these numbers.
 *
 * The storage sizes will differ from board to board due to differences in
 * available storage space.
 */
#ifdef BOARD_BLOONCHIPPER
#define FPSENSOR_MAX_IMAGE_SIZE (26260)
#define FPSENSOR_MAX_ALGORITHM_MAX_TEMPLATE_SIZE (5092)
#define FPSENSOR_MAX_FINGER_COUNT 5
#elif defined(BOARD_DARTMONKEY)
#define FPSENSOR_MAX_IMAGE_SIZE (35460)
#define FPSENSOR_MAX_ALGORITHM_MAX_TEMPLATE_SIZE (47552)
#define FPSENSOR_MAX_FINGER_COUNT 5
#endif
#endif /* __CROS_EC_FPSENSOR_LIMITS_H */
