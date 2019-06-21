# -*- makefile -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build
#
CHIP:=mt_scp
CHIP_VARIANT:=mt8183

board-y=board.o
board-y+=test_hotword_detection.o

HOTWORD_PRIVATE_LIB:=private/libcortexm4_google_hotword_dsp_api.a
ifneq ($(wildcard $(HOTWORD_PRIVATE_LIB)),)
LDFLAGS_EXTRA+=$(HOTWORD_PRIVATE_LIB)
CFLAGS+=-fsingle-precision-constant
HAVE_PRIVATE_AUDIO_CODEC_WOV_LIBS:=y
endif
