#!/bin/bash
# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Static Variables
COMMIT_MSG=$(git log -1 --pretty=format:%b "${PRESUBMIT_COMMIT}")
mapfile -t FILES < <(echo "${PRESUBMIT_FILES}")
EXIT_STATUS=0
EDITED_GPIO_FILE=
BOARD_NAME=

# User defined variables
GPIO_FILE="gpio.inc"
TOP_CONFIG_FILE="include/config.h"
FLAG="CONFIG_SYSTEM_UNLOCKED"

commit_msg_check() {
    local COMMIT_TEST_MSG
    local EXIT_STAT

    COMMIT_TEST_MSG=$(echo "${COMMIT_MSG}" |
        grep -h "TEST=" | sed s/TEST=//)
    EXIT_STAT=0

    COMMIT_TEST_MSG="${COMMIT_TEST_MSG,,}"
    BOARD_NAME_LOWER="${BOARD_NAME,,}"
    echo -en "\033[31;1m"
    if [[ "${COMMIT_TEST_MSG}" != *"ro"* ]]; then
        echo "You made an edit in ${EDITED_GPIO_FILE} while" \
            "the system is locked without testing RO"
        echo "Perform a test against RO to prevent issues."
        EXIT_STAT=1
    elif [[ "${COMMIT_TEST_MSG}" != *"${BOARD_NAME_LOWER}"* ]]; then
        echo "You made an edit in ${EDITED_GPIO_FILE} while" \
            "the system is locked without testing RO" \
            "against ${BOARD_NAME}."
        echo "Perform a test against RO to prevent issues."
        EXIT_STAT=1
    fi
    echo -en "\033[0m"
    return "${EXIT_STAT}"
}


for FILE in "${FILES[@]}"
do
    BASENAME=$(basename "${FILE}")

    # Test if we use GPIO
    if [[ "${BASENAME}" != "${GPIO_FILE}" ]]; then
        continue
    fi

    EDITED_GPIO_FILE="${FILE}"
    BOARD_NAME="$(basename "$(dirname "${FILE}")")"
    BOARD_CHECK="$(basename "$(dirname "$(dirname "${FILE}")")")"

    # Two levels up should be the board folder
    # If not it indicates that this isn't the file we think it is
    if [[ "${BOARD_CHECK}" != "board" ]]; then
        echo "Warning: gpio.inc altered, but couldn't" \
            "detect what board it's from."
        continue
    fi

    BOARD_H=$(dirname "${FILE}")"/board.h"

    # Check if this flag is defined in the top lvl config
    if grep -hq "#define ${FLAG}" "${TOP_CONFIG_FILE}"; then
        if grep -hq "#undef ${FLAG}" "${BOARD_H}"; then
            commit_msg_check
            ! (( "${?}" | "${EXIT_STATUS}" ));
            EXIT_STATUS="${?}"
        else
            continue # by default the flag is defined
        fi

    else # Top lvl flag isn't explicitly stated or is #undef
        if grep -hq "#define ${FLAG}" "${BOARD_H}"; then
            continue # by default the flag is defined
        else
            commit_msg_check
            ! (( "${?}" | "${EXIT_STATUS}" ));
            EXIT_STATUS="${?}"
        fi
    fi


done

exit "${EXIT_STATUS}"
