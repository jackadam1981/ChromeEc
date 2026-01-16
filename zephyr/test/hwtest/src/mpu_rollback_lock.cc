/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_error_hook.h>

LOG_MODULE_REGISTER(mpu_rollback_lock, LOG_LEVEL_INF);

ZTEST_SUITE(mpu_rollback_lock, NULL, NULL, NULL, NULL, NULL);

#ifdef __cplusplus
extern "C" {
#endif

uint32_t unlock_rollback(void);

int mpu_lock_rollback(int lock)
{
	return -ENOENT;
}

#ifdef __cplusplus
}
#endif

void ztest_post_fatal_error_hook(unsigned int reason,
				 const struct arch_esf *pEsf)
{
	zassert_equal(reason, K_ERR_KERNEL_PANIC);
	ztest_set_fault_valid(false);
}

void send_unlock_rollback(void)
{
	ztest_set_fault_valid(true);

	unlock_rollback();

	/* Should never reach this. */
	zassert_unreachable();
}

ZTEST(mpu_rollback_lock, test_mpu_rollback_lock_crash)
{
	send_unlock_rollback();

	/* Should never reach this. */
	zassert_unreachable();
}
