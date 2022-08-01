/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#ifndef AMS_MATH_H
#define AMS_MATH_H

#include <stdint.h>

#define AMS_PI 3.14159265358979323846264338327950288

int get_4d_unit_vector(int16_t *buffer, int buf_len, float *unit_vec);
float dot_product(float *a, float *b, int len);
float get_mean(uint16_t *data, int len);
int rounded_divide(long long a, int b);
int sqrt_long_long(long long x);

#endif
