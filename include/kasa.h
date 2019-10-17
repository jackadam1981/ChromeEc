/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Kasa sphere fit algorithm */

#ifndef __CROS_EC_KASA_H
#define __CROS_EC_KASA_H

#include "vec3.h"

struct kasa_fit {
	float acc_x, acc_y, acc_z, acc_w;
	float acc_xx, acc_xy, acc_xz, acc_xw;
	float acc_yy, acc_yz, acc_yw;
	float acc_zz, acc_zw;
	uint32_t nsamples;
};

/**
 * Resets the kasa_fit data structure (sets all variables to zero).
 *
 * @param kasa Pointer to the struct that should be reset.
 */
void kasa_reset(struct kasa_fit *kasa);

/**
 * Add a new sample to the kasa_fit structure.
 *
 * @param x The X component of the new sample.
 * @param y The Y component of the new sample.
 * @param z the Z component of the new sample.
 */
void kasa_accumulate(struct kasa_fit *kasa, float x, float y, float z);

/**
 * Compute the current center/radius from the kasa_fit structure.
 *
 * @param kasa Pointer to the struct that should be used for the calculation.
 * @param bias Pointer to the start of a float[3] to save the computed center.
 * @param radius Pointer to a float that will hold the computed radius.
 */
void kasa_compute(struct kasa_fit *kasa, fpv3_t bias, float *radius);

#endif /* __CROS_EC_KASA_H */


