
/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_BODY_DETECTION_H
#define __CROS_EC_BODY_DETECTION_H

/*
 * macro for board config,
 * will move to the board config if some board use body detection
 */
#define CONFIG_BODY_DETECTION_SENSOR           BASE_ACCEL
#define CONFIG_BODY_DETECTION_MAX_WINDOW_SIZE  200 /* max sensor odr (Hz) */
#define CONFIG_BODY_DETECTION_VAR_THRESHOLD         600 /* (mm/s^2)^2 */
#define CONFIG_BODY_DETECTION_CONFIDENCE_DELTA      525 /* (mm/s^2)^2 */
#define CONFIG_BODY_DETECTION_ON_BODY_CON           50  /* % */
#define CONFIG_BODY_DETECTION_OFF_BODY_CON          10  /* % */
#define CONFIG_BODY_DETECTION_STATIONARY_DURATION   (15 * SECOND)

void body_detection_reset(void);
int body_detect(void);

#endif /* __CROS_EC_BODY_DETECTION_H */
