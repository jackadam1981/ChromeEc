/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Misc utilities for fingerprint management */

#ifndef __CROS_EC_FPSENSOR_UTILS_H
#define __CROS_EC_FPSENSOR_UTILS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CPRINTF(format, args...) cprintf(CC_FP, format, ##args)
#define CPRINTS(format, args...) cprints(CC_FP, format, ##args)

#endif /* __CROS_EC_FPSENSOR_UTILS_H */
