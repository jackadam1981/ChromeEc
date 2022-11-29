# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set(PACKAGE_VERSION "0.1.0")

# Check whether the requested PACKAGE_FIND_VERSION is compatible
if("${PACKAGE_VERSION}" VERSION_LESS "${PACKAGE_FIND_VERSION}")
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
  set(PACKAGE_VERSION_COMPATIBLE TRUE)
  if ("${PACKAGE_VERSION}" VERSION_EQUAL "${PACKAGE_FIND_VERSION}")
    set(PACKAGE_VERSION_EXACT TRUE)
  endif()
endif()