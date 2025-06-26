# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

_zcbor_cur_dir:=$(dir $(lastword $(MAKEFILE_LIST)))

includes-y+=$(_zcbor_cur_dir)/include
dirs-y+=$(_zcbor_cur_dir)/src

zcbor-y=src/zcbor_common.o src/zcbor_encode.o src/zcbor_decode.o
