# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")
project(krabby)

add_subdirectory(${PLATFORM_EC}/zephyr/test/test_utils test_utils)

zephyr_include_directories("${PLATFORM_EC}/zephyr/projects/corsola/include")

target_sources(app PRIVATE
	src/stubs.c
<<<<<<< HEAD   (c8af73 Revert "Merge remote-tracking branch cros/main into factory-)
	${PLATFORM_EC}/zephyr/projects/corsola/src/krabby/usbc_config.c)
=======
	${PLATFORM_EC_PROGRAM_DIR}/corsola/src/ite_usbc.c
	${PLATFORM_EC_PROGRAM_DIR}/corsola/src/ite_usb_pd_policy.c)
>>>>>>> BRANCH (2c3025 PCHG: Print next event in pchg console command)

target_sources_ifdef(CONFIG_TEST_KRABBY
	app PRIVATE
	src/charger_workaround.c
	src/usb_mux_init.c
<<<<<<< HEAD   (c8af73 Revert "Merge remote-tracking branch cros/main into factory-)
	${PLATFORM_EC}/zephyr/projects/corsola/src/krabby/charger_workaround.c)
=======
	src/usbc_config.c
	${PLATFORM_EC_PROGRAM_DIR}/corsola/src/ite_charger_workaround.c)
>>>>>>> BRANCH (2c3025 PCHG: Print next event in pchg console command)

target_sources_ifdef(CONFIG_TEST_TENTACRUEL
	app PRIVATE
	src/fake.c
	src/temp_tentacruel.c
<<<<<<< HEAD   (c8af73 Revert "Merge remote-tracking branch cros/main into factory-)
	${PLATFORM_EC}/zephyr/projects/corsola/src/krabby/temp_tentacruel.c)
=======
	src/fwconfig_tentacruel.c
	${PLATFORM_EC_PROGRAM_DIR}/corsola/tentacruel/src/temp.c
	${PLATFORM_EC_PROGRAM_DIR}/corsola/tentacruel/src/sensor.c)
>>>>>>> BRANCH (2c3025 PCHG: Print next event in pchg console command)
