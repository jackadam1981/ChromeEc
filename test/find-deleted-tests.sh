#!/bin/bash

# Usage: assoc-add-keys <associate_array_name> [item1 [item2...]]
assoc-add-keys() {
  local -n arr="${1}"
  shift

  for key; do
    arr["${key}"]="${key}"
  done
}

TESTS=( $(ls *.tasklist | tr 'a-z' 'A-Z') )

declare -A TESTSA
assoc-add-keys TESTSA "${TESTS[@]%.TASKLIST}"

echo "# Tests Available:"
echo "${TESTSA[@]}" | tr ' ' '\n' | column

echo "# Tests Described in test_config.h without Test:"
for test_ref in $(grep -oP '#ifdef TEST_\K\w+' test_config.h | sort -u); do
  if [[ -z "${TESTSA[${test_ref}]}" ]]; then
    echo "TEST_${test_ref}"
  fi
done
