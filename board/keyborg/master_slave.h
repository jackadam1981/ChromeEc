/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Master/slave identification */

#ifndef __BOARD_KEYBORG_MASTER_SLAVE_H
#define __BOARD_KEYBORG_MASTER_SLAVE_H

int master_slave_is_master(void);

int master_slave_sync(int timeout_ms);

int master_slave_init(void);

#endif /* __BOARD_KEYBORG_MASTER_SLAVE_H */
