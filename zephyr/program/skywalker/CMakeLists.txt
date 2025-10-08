# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.20.5)
find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")

target_sources(app PRIVATE
	${PLATFORM_EC_PROGRAM_DIR}/skywalker/src/dp_alt_mode.c
	${PLATFORM_EC_PROGRAM_DIR}/skywalker/src/i2c.c
	${PLATFORM_EC_PROGRAM_DIR}/skywalker/src/hibernate.c
	${PLATFORM_EC_PROGRAM_DIR}/skywalker/src/hooks.c
	${PLATFORM_EC_PROGRAM_DIR}/skywalker/src/backlight.c)

if(DEFINED CONFIG_BOARD_SKYWALKER)
	project(skywalker)
	add_subdirectory(skywalker)
elseif(DEFINED CONFIG_BOARD_LUUKE)
	project(luuke)
	add_subdirectory(luuke)
elseif(DEFINED CONFIG_BOARD_OBIWAN)
	project(obiwan)
	add_subdirectory(obiwan)
elseif(DEFINED CONFIG_BOARD_YODA)
	project(yoda)
	add_subdirectory(yoda)
elseif(DEFINED CONFIG_BOARD_ANAKIN)
	project(anakin)
	add_subdirectory(anakin)
elseif(DEFINED CONFIG_BOARD_BAZE)
	project(baze)
	add_subdirectory(baze)
elseif(DEFINED CONFIG_BOARD_TARKIN)
	project(tarkin)
	add_subdirectory(tarkin)
<<<<<<< HEAD   (1a9478ed621654e0af34b8857313d074ac0aa5cf pujjolo: Change EN_USB_A0_VBUS to correct pin)
||||||| BASE   (d29cbb1c7df42374a2dfcadcf432005a08cff92d bluey: Add initial devicetree configs for keyboard)
elseif(DEFINED CONFIG_BOARD_PADME)
	project(padme)
	add_subdirectory(padme)
=======
elseif(DEFINED CONFIG_BOARD_PADME)
	project(padme)
	add_subdirectory(padme)
elseif(DEFINED CONFIG_BOARD_GROGU)
	project(grogu)
	add_subdirectory(grogu)
>>>>>>> CHANGE (01ff989710682061c1a6ace9071283ddea094465 Grogu: Initial zephyr config for grogu project)
endif()
