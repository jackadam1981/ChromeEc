# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

bus0=$(lsusb -d 0403:6001 | cut -d ' ' -f 2)
bus1=$(lsusb -d 0403:6001 | cut -d ' ' -f 4 | sed s/://)
bus=/dev/bus/usb/$bus0/$bus1
sudo chmod 666 $bus

pkill ec_uartd

sleep .1
build/discovery/util/ftdi_gpio c 1
sleep .1
build/discovery/util/ftdi_gpio c 3
sleep .1

./build/discovery/util/ec_uartd x 2> /tmp/uartd_pid &
UARTD_PID=$!
sleep 1
UART=$(head -1 /tmp/uartd_pid | cut -d ' ' -f 4)
echo UART = ${UART}
./build/discovery/util/stm32mon -d ${UART} -u -w ./build/discovery/ec.RO.flat

sleep .5
build/discovery/util/ftdi_gpio c 0
sleep .1
build/discovery/util/ftdi_gpio c 2
sleep .1
build/discovery/util/ftdi_gpio n
sleep .1

