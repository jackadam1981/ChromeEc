#!/bin/bash -e
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is implemented similar to is_enabled_error.sh

TEST_DIR="$(dirname "${BASH_SOURCE[0]}")"

TEST_CMD="$(cat "${TEST_DIR}/RO/test/static_requires_error.o.cmd")"

TEST_ERROR_COUNT=0

for test_value in 0 1 2 A "5 + 5"; do
  echo -n "Running TEST_VALUE=${test_value}..."
  TEST_CMD_COMPLETE="${TEST_CMD} \"-DTEST_VALUE=${test_value}\""
  if BUILD_OUTPUT="$(sh -c "$TEST_CMD_COMPLETE" 2>&1)"; then
    echo "Fail"
    echo "Compilation should not have succeeded for TEST_VALUE=${test_value}"
    echo "$BUILD_OUTPUT"
    TEST_ERROR_COUNT=$((TEST_ERROR_COUNT+1))
    continue
  fi

  # BUILD_ASSERT does not give us a nice way to provide an error message
  # like we can with __attribute__((error(msg))) in IS_ENABLED, so no
  # specific message to test for here :(
done

if [[ $TEST_ERROR_COUNT -eq 0 ]]; then
  echo "Pass!"
else
  echo "Fail! (${TEST_ERROR_COUNT} tests)"
fi
