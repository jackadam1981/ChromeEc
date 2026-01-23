# Copyright 2017 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

# the IC is STmicro STM32H743
CHIP:=stm32
CHIP_FAMILY:=stm32h7
CHIP_VARIANT:=stm32h7x3

# Don't forget that the board build.mk is included more than once to allow
# conditional variables to be realized. This means that we need to redefine all
# variable or the "+=" lines will compound.
board-y=
board-rw=ro_workarounds.o board_rw.o
board-ro=board_ro.o
board-rw+=fpsensor_detect_rw.o
board-y+=fpsensor_detect.o

# Do not build rsa test because this board uses RSA exponent 3 and the rsa test
# will fail on device.
test-list-y=\
       abort \
       assert_builtin \
       assert_stdlib \
       boringssl_crypto \
       cortexm_fpu \
       crc \
       debug \
       exception \
       exit \
       flash_physical \
       flash_write_protect \
       fpsensor_crypto \
       fpsensor_hw \
       ftrapv \
       global_initialization \
       libc_printf \
       libcxx \
       mpu \
       null_pointer \
       panic \
       panic_data \
       printf \
       queue \
       restricted_console \
       rng_benchmark \
       rollback \
       rollback_entropy \
       rollback_lock_panic \
       rsa3 \
       rtc \
       scratchpad \
       stdlib \
       system_is_locked \
       timer \
       timer_dos \
       tpm_seed_clear \
       uart \
       unaligned_access \
       unaligned_access_benchmark \
       utils \
       utils_str \
       watchdog \

# Note that this variable includes the trailing "/"
_nocturne_fp_cur_dir:=$(dir $(lastword $(MAKEFILE_LIST)))
-include $(_nocturne_fp_cur_dir)../../../ec-private/board/nocturne_fp/build.mk
