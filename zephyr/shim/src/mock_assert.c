/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/ztest.h>

#include "builtin/assert.h"

DEFINE_FAKE_VOID_FUNC(ASSERT, bool);

static void default_assert_custom_function(bool condition)
{
	__ASSERT_NO_MSG(condition);
}

static void mock_assert_rule_before(const struct ztest_unit_test *test,
				  void *data)
{
	ARG_UNUSED(test);
	ARG_UNUSED(data);

	RESET_FAKE(ASSERT);
	ASSERT_fake.custom_fake = default_assert_custom_function;
}

ZTEST_RULE(mock_assert_rule, mock_assert_rule_before, NULL);
