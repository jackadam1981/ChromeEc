/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __COMMON_DISABLE_CLANG_OPTIMIZATIONS_H
#define __COMMON_DISABLE_CLANG_OPTIMIZATIONS_H

/* Include guards aren't necessary as the pragma is idempotent. Adding them to
 * maintain include guard patterns
 */

/**
 * When compiling the EC with the -g option we get many debug symbols but can
 * lose information due to compile-time optimizations. Unfortunately, if we
 * set optimizations to be -O0 it also removes LTO optimization, which
 * breaks the build due to usages of IS_ENABLED. However, debugger friendly
 * unoptimized code can still be gained at the pre-link stage by inserting a
 * "pragma clang optimize off" in the beginning of each file.
 *
 * This header should be used with the --include option clang compiler flag to
 * add the pragma to each source before compiling.
 *
 * See: clang.llvm.org/docs/ClangCommandLineReference.html for details.
 */

#pragma clang optimize off

#endif /* __COMMON_DISABLE_CLANG_OPTIMIZATIONS_H */
