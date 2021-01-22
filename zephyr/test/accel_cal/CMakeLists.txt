# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
set(BOARD native_posix)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(accel_cal)

# Need to ensure that we are including only zephyr definitions in include files
zephyr_compile_definitions("CONFIG_ZEPHYR")
zephyr_compile_definitions("CONFIG_SHIMMED_TASKS")

# Ensure that we get the definitions from test_config.h
zephyr_compile_definitions("TEST_ACCEL_CAL")

# We need to include the EC include directory and this local test directory
# for the task defines
zephyr_include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${PLATFORM_EC}/zephyr/shim/include"
  "${PLATFORM_EC}/fuzz"
  "${PLATFORM_EC}/test"
  "${PLATFORM_EC}/include")

# Include test file and unit under test
target_sources(app PRIVATE
	"${PLATFORM_EC}/test/accel_cal.c"
	"${PLATFORM_EC}/common/accel_cal.c"
	"${PLATFORM_EC}/common/kasa.c"
	"${PLATFORM_EC}/common/mat44.c"
	"${PLATFORM_EC}/common/math_util.c"
	"${PLATFORM_EC}/common/newton_fit.c"
#	"${PLATFORM_EC}/common/queue.c"
#	"${PLATFORM_EC}/common/queue_policies.c"
	"${PLATFORM_EC}/common/stillness_detector.c"
	"${PLATFORM_EC}/common/vec3.c")