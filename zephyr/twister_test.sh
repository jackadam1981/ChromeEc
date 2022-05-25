#!/bin/bash

/mnt/host/source/src/third_party/zephyr/main/scripts/twister \
        -T ../../../platform/ec/zephyr/test/math/ \
        -x=ZEPHYR_MODULES="/mnt/host/source/src/third_party/zephyr/cmsis;/mnt/host/source/src/third_party/zephyr/nanopb;/mnt/host/source/src/third_party/zephyr/hal_stm32;/mnt/host/source/src/platform/ec" \
        -x=CMAKE_EXPORT_COMPILE_COMMANDS="ON" \
        -x=DTS_ROOT="/mnt/host/source/src/platform/ec/zephyr" \
        -x=SYSCALL_INCLUDE_DIRS="/mnt/host/source/src/platform/ec/zephyr/include/drivers" \
        -x=DTC_OVERLAY_FILE="/mnt/host/source/src/platform/ec/zephyr/dts/board-overlays/native_posix.dts" \
        --ninja \
        -p native_posix
