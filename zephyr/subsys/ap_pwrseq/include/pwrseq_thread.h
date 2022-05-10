/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __PWRSEQ_THREAD_H__
#define __PWRSEQ_THREAD_H__

#include <stdbool.h>

/** Return true if executing in the power sequencing thread. */
bool is_in_pwrseq_thread(void);

#endif /* __PWRSEQ_THREAD_H__ */
