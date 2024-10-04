#!/bin/bash
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

commit=$1
shift
files=$*

if [[ ${files} == *"usb_updater"* ]]; then
  if ! git log -n 1 "${commit}" | \
     grep -q "TEST=make tast TAST_EXPR=gscdevboard.GSCFactoryUpdate"; then
    echo 'Run and add "TEST=make tast' \
         'TAST_EXPR=gscdevboard.GSCFactoryUpdate.*"' \
         'to commit msg for gsctool changes/'
    exit 1
  fi
fi
