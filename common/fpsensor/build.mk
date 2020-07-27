# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build for fingerprint sensor

# Note that this variable includes the trailing "/"
_fpsensor_dir:=$(dir $(lastword $(MAKEFILE_LIST)))

all-obj-$(HAS_TASK_FPSENSOR)+=$(_fpsensor_dir)fpsensor_state.o
all-obj-$(HAS_TASK_FPSENSOR)+=$(_fpsensor_dir)fpsensor_crypto.o
<<<<<<< HEAD   (6f5daa driver/opt3100: Set min/max frequency that match the driver)
ifneq ($(CONFIG_SPI_FP_PORT),)
all-obj-$(HAS_TASK_FPSENSOR)+=$(_fpsensor_dir)fpsensor.o
endif
ifeq ($(HAS_MOCK_FPSENSOR),)
all-obj-$(HAS_TASK_FPSENSOR)+=$(_fpsensor_dir)fpsensor_stubs.o
endif
=======
all-obj-$(HAS_TASK_FPSENSOR)+=$(_fpsensor_dir)fpsensor.o
all-obj-$(HAS_TASK_CONSOLE)+=$(_fpsensor_dir)fpsensor_detect_strings.o
>>>>>>> BRANCH (40d09f ectool: motionsense: add commands for fast/manual offset com)
