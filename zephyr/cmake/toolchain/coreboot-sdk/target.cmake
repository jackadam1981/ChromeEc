# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Coreboot SDK uses GCC
set(COMPILER gcc)
set(LINKER ld)
set(BINTOOLS gnu)

# Mapping of Zephyr architecture -> coreboot-sdk toolchain
set(CROSS_COMPILE_TARGET_arm    arm-eabi)
set(CROSS_COMPILE_TARGET_riscv  riscv64-elf)
set(CROSS_COMPILE_TARGET_x86    i386-elf)

set(CROSS_COMPILE_TARGET        ${CROSS_COMPILE_TARGET_${ARCH}})

if("${ARCH}" STREQUAL "arm" AND CONFIG_ARM64)
  set(CROSS_COMPILE_TARGET      aarch64-elf)
elseif("${ARCH}" STREQUAL "x86" AND CONFIG_X86_64)
  set(CROSS_COMPILE_TARGET      x86_64-elf)
endif()

set(CC gcc)
set(CROSS_COMPILE "/opt/coreboot-sdk/bin/${CROSS_COMPILE_TARGET}-")

SET(CMAKE_AR         "${CROSS_COMPILE}gcc-ar")
set(CMAKE_NM         "${CROSS_COMPILE}nm")
set(CMAKE_OBJCOPY    "${CROSS_COMPILE}objcopy")
set(CMAKE_OBJDUMP    "${CROSS_COMPILE}objdump")
set(CMAKE_RANLIB     "${CROSS_COMPILE}ranlib")
set(CMAKE_READELF    "${CROSS_COMPILE}readelf")

if (DEFINED CONFIG_LTO)
  # Enable link time optimization (LTO) by the linker.
  # LTO must also be enabled by the compiler, however the Zephyr kernel
  # does not compile with LTO enabled. So LTO is only enabled for the
  # Chromium OS based sources.
  # See https://github.com/zephyrproject-rtos/zephyr/issues/2112

  # TODO: Enable LTO for all sources when Zephyr supports it.
  #SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")
  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
  SET(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
  SET(CMAKE_C_ARCHIVE_FINISH   true)
endif()

# On ARM, we don't use libgcc: It's built against a fixed target (e.g.
# used instruction set, ABI, ISA extensions) and doesn't adapt when
# compiler flags change any of these assumptions. Use our own mini-libgcc
# instead.
if("${ARCH}" STREQUAL "arm")
  set(no_libgcc True)
endif()
