#!/bin/bash
#
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

port=22
pass="test0000"

if [ $# -lt 1 ]; then
echo "run_remote_unit_test.sh target [port] [password]"
echo ""
echo "The script compiles remote unit tests, copies it to the target, loads"
echo "ucsi_um_test module and finally executes remote unit tests on the target."
echo ""
echo "Default port is 22."
echo "Default password is 'test0000'."
exit
fi

target=$1
[ $# -gt 1 ] && port=$2
[ $# -gt 2 ] && pass=$3

echo "Compile remote unit tests ..."
cros_sdk bash --login -c 'cd ../platform/ec/extra/um_ppm; make remote_tests'

echo "Copy remote unit tests to target ${target} port ${port} ..."
sshpass -p "${pass}" scp -P "${port}" remote_unit_tests "${target}":~/

# load ucsi_um_test module
sshpass -p "${pass}" ssh -p "${port}" "${target}" "modprobe ucsi_um_test"

echo "Execute remote unit tests on target ${target} port ${port} ..."
sshpass -p "${pass}" ssh -p "${port}" "${target}" /root/remote_unit_tests
