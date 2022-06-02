#!/bin/bash

export EC_DIR=/mnt/host/source/src/platform/ec

export TOOLCHAIN_ROOT=${EC_DIR}/zephyr

export ZEPHYR_DIR=/mnt/host/source/src/third_party/zephyr
export ZEPHYR_BASE=${ZEPHYR_DIR}/main
export ZEPHYR_TOOLCHAIN_VARIANT=llvm

"${ZEPHYR_DIR}"/main/scripts/twister \
        -T "${EC_DIR}/zephyr/test/drivers/" \
        -x=ZEPHYR_MODULES="${ZEPHYR_DIR}/cmsis;${ZEPHYR_DIR}/nanopb;${ZEPHYR_DIR}/hal_stm32;${EC_DIR}" \
        -x=CMAKE_EXPORT_COMPILE_COMMANDS="ON" \
        -x=DTS_ROOT="${EC_DIR}/zephyr" \
        -x=SYSCALL_INCLUDE_DIRS="${EC_DIR}/zephyr/include/drivers" \
        --ninja --clobber --disable-suite-name-check \
        -p native_posix
