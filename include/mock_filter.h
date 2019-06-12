/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Filter mocklist in *.mocklist, depending on section (RO/RW), or
 * TEST/CTS build.
 */

#ifndef __CROS_EC_MOCK_FILTER_H
#define __CROS_EC_MOCK_FILTER_H

/* If included directly from Makefile, dump mock list. */
#ifdef _MAKEFILE
#define MOCK(n) n
CONFIG_MOCK_LIST CONFIG_TEST_MOCK_LIST CONFIG_CTS_MOCK_LIST
#endif


#endif /*  __CROS_EC_MOCK_FILTER_H */
