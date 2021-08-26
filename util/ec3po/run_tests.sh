#!/bin/bash
#
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

my_dir="$(dirname -- "$0")"

PYTHONPATH="$my_dir/.." python3 -m unittest \
    ec3po.console_unittest \
    ec3po.interpreter_unittest \
    && touch "$my_dir/.tests-passed"
