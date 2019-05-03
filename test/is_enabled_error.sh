#!/bin/bash -ex
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

TEST_DIR="$(dirname "${BASH_SOURCE[0]}")"

TEST_CMD="$(cat "${TEST_DIR}/RO/test/is_enabled_error.o.cmd")"

for test_value in 1 2 5; do
	TEST_CMD_COMPLETE="${TEST_CMD} -DTEST_VALUE=${test_value}"
	if BUILD_OUTPUT="$(sh -c "$TEST_CMD_COMPLETE" 2>&1)"; then
		echo "Compilation should not have succeeded for" \
		     "TEST_VALUE=${test_value}"
		echo "$BUILD_OUTPUT"
		exit 1
	fi

	EXPECTED_ERROR="CONFIG_VALUE must be <blank>, or not defined"
	if grep -q "$EXPECTED_ERROR" <<< "$BUILD_OUTPUT"; then
		echo "Pass!"
	else
		echo "Expected to find: $EXPECTED_ERROR"
		echo "Actual error:"
		echo "$BUILD_OUTPUT"
		exit 1
	fi
done
