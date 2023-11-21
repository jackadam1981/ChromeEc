/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * @file
 * @brief PD task to configure USB-C Alternate modes on Intel SoC.
 */

#ifndef __CROS_EC_PD_TASK_INTEL_ALTMODE_H
#define __CROS_EC_PD_TASK_INTEL_ALTMODE_H

enum intel_altmode_event {
	INTEL_ALTMODE_EVENT_FORCE,
	INTEL_ALTMODE_EVENT_INTERRUPT,
	INTEL_ALTMODE_EVENT_COUNT
};

/**
 * @brief Starts the Intel Alternate Mode configuration thread.
 */
void intel_altmode_task_start(void);
void intel_altmode_post_event(enum intel_altmode_event event);
void suspend_pd_task(void);
void resume_pd_task(void);

#endif /* __CROS_EC_PD_TASK_INTEL_ALTMODE_H */
