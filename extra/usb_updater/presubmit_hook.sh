#!/bin/bash
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


commit=$1
shift
files=$@

if [[ $files == *"/extra/usb_updater/"* ]]; then
  git log -n 1 $commit | \
    grep "TEST=make tast TAST_EXPR=gscdevboard.GSCFactoryUpdate"
  if [[ $? -ne 0 ]]; then
    echo 'Run and add "TEST=make tast ' \
         'TAST_EXPR=gscdevboard.GSCFactoryUpdate.*"' \
         'to commit msg for gsctool changes/'
    exit 1
  fi
fi
