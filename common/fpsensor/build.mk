# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build for fingerprint sensor

# Note that this variable includes the trailing "/"
_fpsensor_dir:=$(dir $(lastword $(MAKEFILE_LIST)))

<<<<<<< HEAD   (ae164f nocturne: Only notify MKBP via hostevent in suspend)
all-obj-$(HAS_TASK_FPSENSOR)+=$(_fpsensor_dir)fpsensor_state.o
=======
all-obj-$(HAS_TASK_FPSENSOR)+=$(_fpsensor_dir)/fpsensor_state.o
all-obj-$(HAS_TASK_FPSENSOR)+=$(_fpsensor_dir)/fpsensor_crypto.o
>>>>>>> CHANGE (b5c6cf fpsensor: Add unit test for derive_encryption_key().)
ifneq ($(CONFIG_SPI_FP_PORT),)
<<<<<<< HEAD   (ae164f nocturne: Only notify MKBP via hostevent in suspend)
all-obj-$(HAS_TASK_FPSENSOR)+=$(_fpsensor_dir)fpsensor.o
=======
all-obj-$(HAS_TASK_FPSENSOR)+=$(_fpsensor_dir)/fpsensor.o
>>>>>>> CHANGE (b5c6cf fpsensor: Add unit test for derive_encryption_key().)
endif
