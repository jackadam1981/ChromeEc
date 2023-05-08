#!/bin/bash
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

find build -name "database.bin" -print0 | xargs -0 \
  python3 ../../third_party/pigweed/pw_tokenizer/py/pw_tokenizer/database.py \
  create --type binary --force --database tokens.bin
