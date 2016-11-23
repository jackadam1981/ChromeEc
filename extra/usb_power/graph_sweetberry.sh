#!/bin/bash

# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


./powerlog.py \
  -b board/marlin/marlin.board -c board/marlin/marlin_vbat.scenario -t 10000  \
  | python streamgraph.py
