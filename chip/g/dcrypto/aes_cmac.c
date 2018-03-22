/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "dcrypto.h"
#include "console.h"

// Given a 128-bit number as bytes, shift to the left by 1
void shiftl_1(const uint8_t *in, uint8_t *out)
{
	int i;
	uint8_t carry = 0;

	for (i = 15; i >= 0; i--) {
		out[i] = in[i] << 1;
		out[i] |= carry;
		carry = (in[i] & 0x80) ? 1 : 0;
	}
}

void xor128(const uint32_t in1[4], const uint32_t in2[4], uint32_t out[4])
{
	int i;

	for (i = 0; i < 4; i++)
		out[i] = in1[i] ^ in2[i];
}

// Retrieve a byte from a byte array, with padding
uint8_t get_byte(const uint8_t *arr, const uint32_t len, int i)
{
	if (i < len)
		return arr[i];
	if (i == len)
		return 0x80;
	return 0;
}
// Retrieve a 32-bit section from a byte array, with padding
uint32_t get_block32(const uint8_t *arr, const uint32_t nBytes, int i)
{
	int j;
	const int Bsize = 4; // 4 bytes per 32-bit block
	uint32_t out = 0;

	for (j = 0; j < Bsize; j++)
		out |= get_byte(arr, nBytes, i*Bsize+j) << (j*8);

	return out;
}

// Retrieve a 128-bit block from a byte array, with padding
void get_block128(const uint8_t *arr, const uint32_t nBytes, int i,
		  uint32_t block[4])
{
	int j;
	const int Bsize = 4; // 4 32-bit blocks per 128-bit block

	for (j = 0; j < Bsize; j++)
		block[j] = get_block32(arr, nBytes, i*Bsize+j);
}

// Wrapper for initializing and calling AES-128
void AES128(const uint8_t *K, const uint32_t in[4], uint32_t out[4])
{
	const uint32_t zero[4] = {0, 0, 0, 0};

	DCRYPTO_aes_init((const uint8_t *)K, 128, (const uint8_t *) zero,
			 CIPHER_MODE_CBC, ENCRYPT_MODE);
	DCRYPTO_aes_block((const uint8_t *) in, (uint8_t *) out);
}

void gen_subkey(const uint8_t *K, uint32_t k1[4], uint32_t k2[4])
{
	uint32_t L[4];
	uint32_t tmp[4];
	const uint32_t *xor_term;
	const uint32_t zero[4] = {0, 0, 0, 0};
	const uint32_t Rb[4] = {0, 0, 0, 0x87000000};

	AES128(K, zero, L);

	xor_term = (L[0] & 0x00000080) ? Rb : zero;
	shiftl_1((const uint8_t *) L, (uint8_t *) tmp);
	xor128(tmp, xor_term, k1);

	xor_term = (k1[0] & 0x00000080) ? Rb : zero;
	shiftl_1((const uint8_t *) k1, (uint8_t *) tmp);
	xor128(tmp, xor_term, k2);
}

void aes_cmac(const uint8_t *K, const uint8_t *M, const uint32_t len,
	      uint32_t T[4])
{
	uint32_t n;
	int i;
	int flag;
	uint32_t k1[4];
	uint32_t k2[4];
	uint32_t M_last[4];
	uint32_t block[4];
	uint32_t Y[4];
	uint32_t X[4] = {0, 0, 0, 0};
	const int log2_Bsize = 4; // 2^4 = 16 bytes per 128-bit block

	// Generate the subkeys K1 and K2
	gen_subkey(K, k1, k2);

	n = len >> log2_Bsize; // floor(len / Bsize)
	flag = ((len - (n << log2_Bsize) == 0) && (n != 0)) ? 1 : 0;
	n = n + (flag ? 0 : 1); // ceil (len / Bsize), or 1 if n = 0

	get_block128(M, len, n-1, block);
	xor128(block, (flag ? k1 : k2), M_last);

	for (i = 0; i < n - 1; i++) {
		get_block128(M, len, i, block);
		xor128(X, block, Y);
		AES128(K, Y, X);
	}

	// TODO: This block is separate from the main loop in the RFC. However,
	// if we set M[n-1] = M_last, then it is equivalent to running the loop
	// for one more step, which might be a nicer way to write it.
	xor128(X, M_last, Y);
	AES128(K, Y, T);
}

// len is the length of M in *bytes*
int aes_cmac_verify(const uint8_t *key, const uint8_t *M, const int len,
		    const uint32_t T[4])
{
	int i;
	uint32_t T2[4];
	int match = 1;

	aes_cmac(key, M, len, T2);

	for (i = 0; i < 4; i++) {
		if (T[i] != T2[i])
			match = 0;
	}
	return match;
}

#ifdef CRYPTO_TEST_SETUP
// For tests
int check_answer(const uint32_t expected[4], uint32_t actual[4])
{
	int i;
	int success = 1;

	for (i = 0; i < 4; i++) {
		if (actual[i] != expected[i])
			success = 0;
	}
	if (success) {
		ccprintf("SUCCESS\n");
	} else {
		ccprintf("FAILURE:\n");
		ccprintf("actual   = 0x%08x 0x%08x 0x%08x 0x%08x\n", actual[0],
			 actual[1], actual[2], actual[3]);
		ccprintf("expected = 0x%08x 0x%08x 0x%08x 0x%08x\n",
			 expected[0], expected[1], expected[2], expected[3]);
	}
	return success;
}

static int command_test_aes_block(int argc, char **argv)
{
	uint32_t actual[4];
	const uint32_t zero[4] = {0, 0, 0, 0};
	const uint32_t K[4] = {0x16157e2b, 0xa6d2ae28, 0x8815f7ab, 0x3c4fcf09};
	const uint32_t expected[4] = {0x0c6bf77d, 0xb399b81a, 0x47f0423e,
				      0x6f541bb9};

	AES128((const uint8_t *) K, zero, actual);
	check_answer(expected, actual);

	return 0;
}

DECLARE_SAFE_CONSOLE_COMMAND(test_aesbk, command_test_aes_block, NULL,
			     "Test AES block in AES-CMAC subkey generation");

static int command_test_subkey_gen(int argc, char **argv)
{
	uint32_t k1[4];
	uint32_t k2[4];
	// K:  2b7e1516 28aed2a6 abf71588 09cf4f3c
	// k1: fbeed618 35713366 7c85e08f 7236a8de
	// k2: f7ddac30 6ae266cc f90bc11e e46d513b
	const uint32_t K[4] = {0x16157e2b, 0xa6d2ae28, 0x8815f7ab, 0x3c4fcf09};
	const uint32_t k1e[4] = {0x18d6eefb, 0x66337135, 0x8fe0857c,
				 0xdea83672};
	const uint32_t k2e[4] = {0x30acddf7, 0xcc66e26a, 0x1ec10bf9,
				 0x3b516de4};

	gen_subkey((const uint8_t *) K, k1, k2);

	ccprintf("Checking K1: ");
	check_answer(k1e, k1);

	ccprintf("\n");

	ccprintf("Checking K2: ");
	check_answer(k2e, k2);

	return 0;
}

DECLARE_SAFE_CONSOLE_COMMAND(test_skgen, command_test_subkey_gen, NULL,
			     "Test AES-CMAC subkey generation");

static int command_test_aes_cmac_1(int argc, char **argv)
{
	uint32_t T[4];
	const uint32_t K[4] = {0x16157e2b, 0xa6d2ae28, 0x8815f7ab, 0x3c4fcf09};
	const uint32_t *M = {0};
	const uint32_t len = 0;
	const uint32_t Te[4] = {0x29691dbb, 0x283759e9, 0x127da37f, 0x4667759b};

	aes_cmac((const uint8_t *)K, (const uint8_t *) M, len, T);
	check_answer(Te, T);

	if (!aes_cmac_verify((const uint8_t *)K, (const uint8_t *) M, len, Te))
		ccprintf("FAILURE: verify returned INVALID\n");

	return 0;
}

DECLARE_SAFE_CONSOLE_COMMAND(test_cmac1, command_test_aes_cmac_1, NULL,
			     "Test AES-CMAC (RFC example 1)");

static int command_test_aes_cmac_2(int argc, char **argv)
{
	uint32_t T[4];
	const uint32_t K[4] = {0x16157e2b, 0xa6d2ae28, 0x8815f7ab, 0x3c4fcf09};
	const uint32_t M[4] = {0xe2bec16b, 0x969f402e, 0x117e3de9, 0x2a179373};
	const uint32_t len = 16;
	const uint32_t Te[4] = {0xb4160a07, 0x44414d6b, 0x9ddd9bf7, 0x7c284ad0};

	aes_cmac((const uint8_t *)K, (const uint8_t *) M, len, T);
	check_answer(Te, T);

	if (!aes_cmac_verify((const uint8_t *)K, (const uint8_t *) M, len, Te))
		ccprintf("FAILURE: verify returned INVALID\n");

	return 0;
}

DECLARE_SAFE_CONSOLE_COMMAND(test_cmac2, command_test_aes_cmac_2, NULL,
			     "Test AES-CMAC (RFC example 2)");

static int command_test_aes_cmac_3(int argc, char **argv)
{
	uint32_t T[4];
	const uint32_t K[4] = {0x16157e2b, 0xa6d2ae28, 0x8815f7ab, 0x3c4fcf09};
	const uint32_t M[10] = {0xe2bec16b, 0x969f402e, 0x117e3de9, 0x2a179373,
				0x578a2dae, 0x9cac031e, 0xac6fb79e, 0x518eaf45,
				0x461cc830, 0x11e45ca3};
	const uint32_t len = 40;
	const uint32_t Te[4] = {0x4767a6df, 0x30e69ade, 0x6132ca30, 0x27c89714};

	aes_cmac((const uint8_t *)K, (const uint8_t *) M, len, T);
	check_answer(Te, T);

	if (!aes_cmac_verify((const uint8_t *)K, (const uint8_t *) M, len, Te))
		ccprintf("FAILURE: verify returned INVALID\n");

	return 0;
}

DECLARE_SAFE_CONSOLE_COMMAND(test_cmac3, command_test_aes_cmac_3, NULL,
			     "Test AES-CMAC (RFC example 3)");

static int command_test_aes_cmac_4(int argc, char **argv)
{
	uint32_t T[4];
	const uint32_t K[4] = {0x16157e2b, 0xa6d2ae28, 0x8815f7ab, 0x3c4fcf09};
	const uint32_t M[16] = {0xe2bec16b, 0x969f402e, 0x117e3de9, 0x2a179373,
				0x578a2dae, 0x9cac031e, 0xac6fb79e, 0x518eaf45,
				0x461cc830, 0x11e45ca3, 0x19c1fbe5, 0xef520a1a,
				0x45249ff6, 0x179b4fdf, 0x7b412bad, 0x10376ce6};
	const uint32_t len = 64;
	const uint32_t Te[4] = {0xbfbef051, 0x929d3b7e, 0x177449fc, 0xfe3c3679};

	aes_cmac((const uint8_t *)K, (const uint8_t *) M, len, T);
	check_answer(Te, T);

	if (!aes_cmac_verify((const uint8_t *)K, (const uint8_t *) M, len, Te))
		ccprintf("FAILURE: verify returned INVALID\n");

	return 0;
}

DECLARE_SAFE_CONSOLE_COMMAND(test_cmac4, command_test_aes_cmac_4, NULL,
			     "Test AES-CMAC (RFC example 4)");
#endif /* CRYPTO_TEST_SETUP */
