#!/bin/bash -e

TEST_DIR="$(dirname "${BASH_SOURCE[0]}")"

if BUILD_OUTPUT="$(sh "${TEST_DIR}/RO/test/is_enabled_error.o.cmd" 2>&1)"; then
	echo "Compilation should not have succeeded"
	echo "$BUILD_OUTPUT"
	exit 1
fi

EXPECTED_ERROR="CONFIG_FIVE must be <blank>, 0, 1, or not defined"
if grep -q "$EXPECTED_ERROR" <<< "$BUILD_OUTPUT"; then
	echo "Pass!"
else
	echo "Expected to find: $EXPECTED_ERROR"
	echo "Actual error:"
	echo "$BUILD_OUTPUT"
	exit 1
fi
