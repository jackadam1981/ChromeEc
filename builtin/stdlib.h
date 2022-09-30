/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STDLIB_H__
#define __CROS_EC_STDLIB_H__

int atoi(const char *nptr);

void qsort(void *ptr, size_t count, size_t size,
	   int (*comp)(const void *, const void *));

void *bsearch(const void *key, const void *base, size_t num, size_t size,
	      int (*compar)(const void *, const void *));

#endif /* __CROS_EC_STDLIB_H__ */
