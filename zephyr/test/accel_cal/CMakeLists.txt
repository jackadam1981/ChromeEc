# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(accel_cal)

#[[
Disclaimer: the following example is pieced together from
https://lemire.me/blog/2020/06/26/gcc-not-nearest/ along with other information
found in GCC documentation.

Brace yourself:
The following flags are needed to pass this test using GNU GCC 7.5 (I have not
tested to see if the flags are needed on other versions of GCC). Note that at
the time of writing, coreboot-sdk uses GCC 10.2.0 which passes this test
without these flags.

This test is very FPU intensive and is susceptible to rounding errors. In GCC
7.5. As an example:

  double x = 50178230318.0;
  double y = 100000000000.0;
  double ratio = x/y;

In this example, we would expect ratio to be 0.50178230318. Instead, using a 64
bit float, it falls between:
* The floating-point number 0.501782303179999944 == 4519653187245114  * 2 ** -53
* The floating-point number 0.501782303180000055 == 4519653187245115  * 2 ** -53

The real mantissa using the same logic should be:
0.50178230318 = 4519653187245114.50011795456 * 2 ** -53

Since the mantissa's decimal is just over 0.5, it should stand to reason that
the correct solution is 0.501782303180000055. To force GCC to round correctly
we must set the following modes:
1. 'sse' - Generate SSE instructions.
2. 'fpmath=sse' - Use scalar floating-point instructions present in the SSE
   instruction set.
3. 'arch=pentium4' - Choose the Pentium4 because it's a stable choice that
   supports SSE and is known to work (there may be other good choices).
]]
set(FLOAT_FLAGS "-msse -mfpmath=sse -march=pentium4")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLOAT_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLOAT_FLAGS}")

# Ensure that we get the definitions from test_config.h
zephyr_compile_definitions("TEST_ACCEL_CAL")

# Include the local test directory for shimmed_test_tasks.h
zephyr_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

# Include test file and unit under test
target_sources(app PRIVATE
	"${PLATFORM_EC}/test/accel_cal.c"
	"${PLATFORM_EC}/common/accel_cal.c"
	"${PLATFORM_EC}/common/kasa.c"
	"${PLATFORM_EC}/common/mat44.c"
	"${PLATFORM_EC}/common/math_util.c"
	"${PLATFORM_EC}/common/newton_fit.c"
	"${PLATFORM_EC}/common/stillness_detector.c"
	"${PLATFORM_EC}/common/vec3.c")
