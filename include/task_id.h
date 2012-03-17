/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* define the task identifier of all compiled tasks */

#ifndef __TASK_ID_H
#define __TASK_ID_H

/* Task identifier (8 bits) */
typedef uint8_t task_id_t;

/* Task ID list generated at compile time.
 * The identifier of a task can be retrieved using the following constant:
 * TASK_ID_<taskname> where <taskname> is the name specified in task.xml.
 */
#include "task_id-impl.h"

#endif  /* __TASK_ID_H */
