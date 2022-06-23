#!/bin/bash
export SEED=${RANDOM}
echo "SEED=$SEED" | tee /tmp/build.log
./build/zephyr/test-drivers/build-singleimage/zephyr/zephyr.exe -seed=$SEED 2>&1 | tee -a /tmp/build.log

