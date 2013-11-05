# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

pkill ec_uartd

./build/discovery/util/ec_uartd 2> /tmp/uartd_pid &
UARTD_PID=$!
sleep .1
UART=$(head -1 /tmp/uartd_pid | cut -d ' ' -f 4)
cu -l ${UART}

