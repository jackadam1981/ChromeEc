/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_SHIMMED_TASKS_H
#define __CROS_EC_SHIMMED_TASKS_H

/*
 * Manually define these HAS_TASK_* defines. There is a build time assert
 * to at least verify we have the minimum set defined correctly.
 */
#define HAS_TASK_TASK_1 1
#define HAS_TASK_TASK_2 1
#define HAS_TASK_TASK_3 1
#define HAS_TASK_TASK_4 1
#define HAS_TASK_TASK_5 1

/* Highest priority on bottom same as in platform/ec */
#define CROS_EC_TASK_LIST \
	CROS_EC_TASK(MTX3C, mutex_random_task, NULL, 384) \
	CROS_EC_TASK(MTX3B, mutex_random_task, NULL, 384) \
	CROS_EC_TASK(MTX3A, mutex_random_task, NULL, 384) \
	CROS_EC_TASK(MTX2, mutex_second_task, NULL, 384) \
	CROS_EC_TASK(MTX1, mutex_main_task, NULL, 384)

#endif /* __CROS_EC_SHIMMED_TASKS_H */
