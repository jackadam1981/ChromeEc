# -*- makefile -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# on-board test binaries build
#

test-list=hello pingpong timer_calib timer_dos timer_jump mutex debounce
#disable: powerdemo

pingpong-y=pingpong.o
powerdemo-y=powerdemo.o
timer_calib-y=timer_calib.o
timer_dos-y=timer_dos.o
mutex-y=mutex.o

chip-mock-debounce-gpio.o=mock_gpio.o
chip-mock-debounce-pwm.o=mock_pwm.o
common-mock-debounce-x86_power.o=mock_x86_power.o
common-mock-debounce-keyboard.o=mock_keyboard.o
common-mock-debounce-i8042.o=mock_i8042.o
