
/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "test_util.h"

extern "C" {
#include "shared_mem.h"
#include "timer.h"
}

#include <array>
#include <cstdio>
#include <cstring>

test_static int benchmark_unaligned_access_memcpy()
{
	int i;
	timestamp_t t0, t1, t2, t3;
	char *buf;
	const int buf_size = 1000;
	const int len = 400;
	const int dest_offset = 500;
	const int iteration = 1000;
	TEST_ASSERT(shared_mem_acquire(buf_size, &buf) == EC_SUCCESS);
	for (i = 0; i < len; ++i)
		buf[i] = i & 0x7f;
	for (i = len; i < buf_size; ++i)
		buf[i] = 0;
	t0 = get_time();
	for (i = 0; i < iteration; ++i) {
		memcpy(buf + dest_offset + 1, buf, len); /* unaligned */
	}
	t1 = get_time();
	TEST_ASSERT_ARRAY_EQ(buf + dest_offset + 1, buf, len);
	ccprintf(" (speed gain: %" PRId64 " ->", t1.val - t0.val);
	t2 = get_time();
	for (i = 0; i < iteration; ++i) {
		memcpy(buf + dest_offset, buf, len); /* aligned */
	}
	t3 = get_time();
	ccprintf(" %" PRId64 " us)\n", t3.val - t2.val);
	TEST_ASSERT_ARRAY_EQ(buf + dest_offset, buf, len);
	return EC_SUCCESS;
}

test_static int benchmark_unaligned_access_array()
{
	timestamp_t t0, t1, t2, t3;
	const int iteration = 1000;
	alignas(int32_t) std::array<int8_t, 100> source_array;
	std::array<int32_t, 20> destination_array;
	constexpr std::array<int32_t, 20> expected_array_unaligned_access = {
		67305985,   134678021,	202050057,  269422093,	336794129,
		404166165,  471538201,	538910237,  606282273,	673654309,
		741026345,  808398381,	875770417,  943142453,	1010514489,
		1077886525, 1145258561, 1212630597, 1280002633, 1347374669
	};
	constexpr std::array<int32_t, 20> expected_array_aligned_access = {
		50462976,   117835012,	185207048,  252579084,	319951120,
		387323156,  454695192,	522067228,  589439264,	656811300,
		724183336,  791555372,	858927408,  926299444,	993671480,
		1061043516, 1128415552, 1195787588, 1263159624, 1330531660
	};
	for (int i = 0; i < source_array.size(); ++i) {
		source_array[i] = static_cast<int8_t>(i);
	}
	t0 = get_time();
	for (int t = 0; t < iteration; ++t) {
		const int32_t *unaligned_source_array_ptr =
			reinterpret_cast<const int32_t *>(source_array.data() +
							  1);
		for (int i = 0; i < destination_array.size(); ++i) {
			destination_array[i] =
				(*unaligned_source_array_ptr++); /* unaligned
								  */
		}
		TEST_ASSERT_ARRAY_EQ(destination_array.data(),
				     expected_array_unaligned_access.data(),
				     destination_array.size());
	}
	t1 = get_time();
	ccprintf(" (speed gain: %" PRId64 " ->", t1.val - t0.val);
	t2 = get_time();
	for (int t = 0; t < iteration; ++t) {
		const int32_t *aligned_source_array_ptr =
			reinterpret_cast<const int32_t *>(source_array.data());
		for (int i = 0; i < destination_array.size(); ++i) {
			destination_array[i] =
				(*aligned_source_array_ptr++); /* aligned
								*/
		}
		TEST_ASSERT_ARRAY_EQ(destination_array.data(),
				     expected_array_aligned_access.data(),
				     destination_array.size());
	}
	t3 = get_time();
	ccprintf(" %" PRId64 " us)\n", t3.val - t2.val);
	return EC_SUCCESS;
}

void run_test(int, const char **)
{
	test_reset();
	RUN_TEST(benchmark_unaligned_access_memcpy);
	RUN_TEST(benchmark_unaligned_access_array);
	test_print_result();
}
