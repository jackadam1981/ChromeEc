# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
