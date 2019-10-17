/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "kasa.h"
#include "mat44.h"
#include <string.h>

#ifndef CONFIG_FPU
#error "Kasa sphere fit algorithm requires CONFIG_FPU"
#endif

void kasa_reset(struct kasa_fit *kasa)
{
	memset(kasa, 0, sizeof(struct kasa_fit));
}

void kasa_accumulate(struct kasa_fit *kasa, float x, float y, float z)
{
	float w = x * x + y * y + z * z;

	kasa->acc_x += x;
	kasa->acc_y += y;
	kasa->acc_z += z;
	kasa->acc_w += w;

	kasa->acc_xx += x * x;
	kasa->acc_xy += x * y;
	kasa->acc_xz += x * z;
	kasa->acc_xw += x * w;

	kasa->acc_yy += y * y;
	kasa->acc_yz += y * z;
	kasa->acc_yw += y * w;

	kasa->acc_zz += z * z;
	kasa->acc_zw += z * w;

	kasa->nsamples += 1;
}

void kasa_compute(struct kasa_fit *kasa, fpv3_t bias, float *radius)
{
	/*    A    *   out   =    b
	 * (4 x 4)   (4 x 1)   (4 x 1)
	 */
	mat44_fp_t A;
	fpv4_t b, out;
	sizev4_t pivot;

	A[0][0] = kasa->nsamples;
	A[0][1] = A[1][0] = kasa->acc_x;
	A[0][2] = A[2][0] = kasa->acc_y;
	A[0][3] = A[3][0] = kasa->acc_z;
	A[1][1] = kasa->acc_xx;
	A[1][2] = A[2][1] = kasa->acc_xy;
	A[1][3] = A[3][1] = kasa->acc_xz;
	A[2][2] = kasa->acc_yy;
	A[2][3] = A[3][2] = kasa->acc_yz;
	A[3][3] = kasa->acc_zz;

	b[0] = -kasa->acc_w;
	b[1] = -kasa->acc_xw;
	b[2] = -kasa->acc_yw;
	b[3] = -kasa->acc_zw;

	mat44_fp_decompose_lup(A, pivot);
	mat44_fp_solve(A, out, b, pivot);

	bias[0] = out[1] * -0.5f;
	bias[1] = out[2] * -0.5f;
	bias[2] = out[3] * -0.5f;

	*radius = fpv3_dot(bias, bias) - out[0];
	*radius = (*radius > 0) ? fp_sqrtf(*radius) : 0.0f;
}
