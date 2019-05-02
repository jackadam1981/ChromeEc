/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MAX32660 Assertion checks for debugging */

#ifndef _MXC_ASSERT_H_
#define _MXC_ASSERT_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup    syscfg
 * @defgroup   mxc_assertions Assertion Checks for Debugging
 * @brief      Assertion checks for debugging.
 * @{
 */
/* **** Definitions **** */
/**
 * @note       To use debug assertions, the symbol @c MXC_ASSERT_ENABLE must be
 *             defined. 
 */
///@cond
#ifdef MXC_ASSERT_ENABLE
/**
 * Macro that checks the expression for true and generates an assertion.
 * @note       To use debug assertions, the symbol @c MXC_ASSERT_ENABLE must be
 *             defined.
 */
#define MXC_ASSERT(expr)                   \
	if (!(expr)) {                           \
		mxc_assert(#expr, __FILE__, __LINE__); \
	}
/**
 * Macro that generates an assertion with the message "FAIL".
 * @note       To use debug assertions, the symbol @c MXC_ASSERT_ENABLE must be
 *             defined.
 */
#define MXC_ASSERT_FAIL() mxc_assert("FAIL", __FILE__, __LINE__);
#else
#define MXC_ASSERT(expr)
#define MXC_ASSERT_FAIL()
#endif
///@endcond
/* **** Globals **** */

/* **** Function Prototypes **** */

/**
 * @brief      Assert an error when the given expression fails during debugging.
 * @param      expr  String with the expression that failed the assertion.
 * @param      file  File containing the failed assertion.
 * @param      line  Line number for the failed assertion.
 * @note       This is defined as a weak function and can be overridden at the
 *             application layer to print the debugging information. 
 *             @code 
 *             printf("%s, file: %s, line %d\n", expr, file, line);
 *             @endcode
 * @note       To use debug assertions, the symbol @c MXC_ASSERT_ENABLE must be
 *             defined. 
 */
void mxc_assert(const char *expr, const char *file, int line);

/**@} end of group MXC_Assertions*/

#ifdef __cplusplus
}
#endif

#endif /* _MXC_ASSERT_H_ */
