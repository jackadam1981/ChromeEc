/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Header file for common math functions. */
#ifndef __CROS_EC_VEC_3_H
#define __CROS_EC_VEC_3_H

#include "math_util.h"

typedef float floatv3_t[3];
typedef fp_t fpv3_t[3];

/*
 * TODO(b:113364863): Remove all float* functions after migration finished.
 * Since that all float* functions can be accessed with fp* functions with
 * CONFIG_FPU enabled.
 */
void floatv3_scalar_mul(floatv3_t v, float c);
float floatv3_dot(const floatv3_t v, const floatv3_t w);
float floatv3_norm_squared(const floatv3_t v);
float floatv3_norm(const floatv3_t v);

/* fixed-point */
void fpv3_scalar_mul(fpv3_t v, fp_t c);
fp_t fpv3_dot(const fpv3_t v, const fpv3_t w);
fp_t fpv3_norm_squared(const fpv3_t v);
fp_t fpv3_norm(const fpv3_t v);
#endif  /* __CROS_EC_VEC_3_H */
