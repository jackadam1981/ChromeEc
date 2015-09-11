/* Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "mat33.h"
#include "math.h"
#include "util.h"

#define kEps 1E-5f

void init_zero_matrix(mat33_t A)
{
	memset(A, 0, sizeof(mat33_t));
}

void init_diagonal_matrix(mat33_t A, float x)
{
	size_t i;
	init_zero_matrix(A);

	for (i = 0; i < 3; ++i) {
		A[i][i] = x;
	}
}

void mat33_scalar_mul(mat33_t A, float c)
{
	size_t i;
	for (i = 0; i < 3; ++i) {
		size_t j;
		for (j = 0; j < 3; ++j) {
			A[i][j] *= c;
		}
	}
}

void mat33_swap_rows(mat33_t A, const size_t i, const size_t j)
{
	const size_t N = 3;
	size_t k;

	if (i == j) {
		return;
	}

	for (k = 0; k < N; ++k) {
		float tmp = A[i][k];
		A[i][k] = A[j][k];
		A[j][k] = tmp;
	}
}

/*
 * Returns the eigenvalues and corresponding eigenvectors of the _symmetric_
 * matrix.
 * The i-th eigenvalue corresponds to the eigenvector in the i-th _row_ of
 * "eigenvecs".
 */
void mat33_get_eigenbasis(mat33_t S, vec3_t eigenvals, mat33_t eigenvecs)
{
	const size_t N = 3;
	size3_t ind;
	size_t i, j, k, l, m;

	for (k = 0; k < N; ++k) {
		ind[k] = mat33_maxind(S, k);
		eigenvals[k] = S[k][k];
	}

	init_diagonal_matrix(eigenvecs, 1.0f);

	for (;;) {
		float y, t, s, c, p, sum;
		m = 0;
		for (k = 1; k + 1 < N; ++k) {
			if (fabsf(S[k][ind[k]]) >
			    fabsf(S[m][ind[m]])) {
				m = k;
			}
		}

		k = m;
		l = ind[m];
		p = S[k][l];

		if (fabsf(p) < kEps) {
			break;
		}

		y = (eigenvals[l] - eigenvals[k]) * 0.5f;

		t = fabsf(y) + sqrtf(p * p + y * y);
		s = sqrtf(p * p + t * t);
		c = t / s;
		s = p / s;
		t = p * p / t;

		if (y < 0.0f) {
			s = -s;
			t = -t;
		}

		S[k][l] = 0.0f;

		eigenvals[k] -= t;
		eigenvals[l] += t;

		for (i = 0; i < k; ++i) {
			mat33_rotate(S, c, s, i, k, i, l);
		}

		for (i = k + 1; i < l; ++i) {
			mat33_rotate(S, c, s, k, i, i, l);
		}

		for (i = l + 1; i < N; ++i) {
			mat33_rotate(S, c, s, k, i, l, i);
		}

		for (i = 0; i < N; ++i) {
			float tmp = c * eigenvecs[k][i] - s * eigenvecs[l][i];
			eigenvecs[l][i] = s * eigenvecs[k][i] + c * eigenvecs[l][i];
			eigenvecs[k][i] = tmp;
		}

		ind[k] = mat33_maxind(S, k);
		ind[l] = mat33_maxind(S, l);

		sum = 0.0f;
		for (i = 0; i < N; ++i) {
			for (j = i + 1; j < N; ++j) {
				sum += fabsf(S[i][j]);
			}
		}

		if (sum < kEps) {
			break;
		}
	}

	for (k = 0; k < N; ++k) {
		m = k;
		for (l = k + 1; l < N; ++l) {
			if (eigenvals[l] > eigenvals[m]) {
				m = l;
			}
		}

		if (k != m) {
			float tmp = eigenvals[k];
			eigenvals[k] = eigenvals[m];
			eigenvals[m] = tmp;

			mat33_swap_rows(eigenvecs, k, m);
		}
	}
}

/* index of largest off-diagonal element in row k */
size_t mat33_maxind(mat33_t A, size_t k)
{
	const size_t N = 3;
	size_t i, m = k + 1;

	for (i = k + 2; i < N; ++i) {
		if (fabsf(A[k][i]) > fabsf(A[k][m])) {
			m = i;
		}
	}

	return m;
}

void mat33_rotate(mat33_t A, float c, float s, size_t k, size_t l, size_t i, size_t j)
{
	float tmp = c * A[k][l] - s * A[i][j];
	A[i][j] = s * A[k][l] + c * A[i][j];
	A[k][l] = tmp;
}


