/* Copyright 2023 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "errno.h"
#include "errno_map.h"
#include "common.h"
#include <zephyr/kernel.h>

int errno_to_ec(int ret)
{
	if (ret == 0) {
		return EC_SUCCESS;
	} else if (ret == -EPERM) {
		return EC_ERROR_ACCESS_DENIED;
	} else if (ret == -E2BIG) {
		return EC_ERROR_PARAM_COUNT;
	} else if (ret == -EAGAIN) {
		return EC_ERROR_TRY_AGAIN;
	} else if (ret == -ENOMEM) {
		return EC_ERROR_MEMORY_ALLOCATION;
	} else if (ret == -EACCES) {
		return EC_ERROR_ACCESS_DENIED;
	} else if (ret == -EBUSY) {
		return EC_ERROR_BUSY;
	} else if (ret == -EINVAL) {
		return EC_ERROR_INVAL;
	} else if (ret == -ETXTBSY) {
		return EC_ERROR_BUSY;
	} else if (ret == -EDOM) {
		return EC_ERROR_INVAL;
	} else if (ret == -ENOLCK) {
		return EC_ERROR_UNAVAILABLE;
	} else if (ret == -ENODATA) {
		return EC_ERROR_UNCHANGED;
	} else if (ret == -ETIME) {
		return EC_ERROR_TIMEOUT;
	} else if (ret == -ENOSR) {
		return EC_ERROR_MEMORY_ALLOCATION;
	} else if (ret == -ENOBUFS) {
		return EC_ERROR_MEMORY_ALLOCATION;
	} else if (ret == -ETIMEDOUT) {
		return EC_ERROR_TIMEOUT;
	} else if (ret == -EINPROGRESS) {
		return EC_SUCCESS_IN_PROGRESS;
	} else if (ret == -EALREADY) {
		return EC_ERROR_BUSY;
	} else if (ret == -ENOTSUP) {
		return EC_ERROR_UNIMPLEMENTED;
	} else if (ret == -EOVERFLOW) {
		return EC_ERROR_OVERFLOW;
	} else {
		return EC_ERROR_UNKNOWN;
	}
}
