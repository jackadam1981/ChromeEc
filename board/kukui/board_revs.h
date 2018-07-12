/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_BOARD_REVS_H
#define __CROS_EC_BOARD_REVS_H

#define KUKUI_REV0 0
#define KUKUI_REV1 1
#define KUKUI_REV2 2
#define KUKUI_REV3 3
#define KUKUI_REV4 4
#define KUKUI_REV5 5
#define KUKUI_REV_LAST    KUKUI_REV5
#define KUKUI_REV_DEFAULT KUKUI_REV0

#if !defined(BOARD_REV)
#define BOARD_REV KUKUI_REV_DEFAULT
#endif

#if BOARD_REV < KUKUI_REV0 || BOARD_REV > KUKUI_REV_LAST
#error "Board revision out of range"
#endif

#endif /* __CROS_EC_BOARD_REVS_H */
