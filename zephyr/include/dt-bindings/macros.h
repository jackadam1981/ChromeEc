/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DT_BINDINGS_MACROS_H_
#define DT_BINDINGS_MACROS_H_

/*
 * Compositional macros to convert binary numbers to C integers.
 * Each B(N) macro is defined in terms of B(N-1).
 */
#define B1(b0) \
	(b0)

#define B2(b1, b0) \
	(((b1) << 1) | B1(b0))

#define B3(b2, b1, b0) \
	(((b2) << 2) | B2(b1, b0))

#define B4(b3, b2, b1, b0) \
	(((b3) << 3) | B3(b2, b1, b0))

#define B5(b4, b3, b2, b1, b0) \
	(((b4) << 4) | B4(b3, b2, b1, b0))

#define B6(b5, b4, b3, b2, b1, b0) \
	(((b5) << 5) | B5(b4, b3, b2, b1, b0))

#define B7(b6, b5, b4, b3, b2, b1, b0) \
	(((b6) << 6) | B6(b5, b4, b3, b2, b1, b0))

#define B8(b7, b6, b5, b4, b3, b2, b1, b0) \
	(((b7) << 7) | B7(b6, b5, b4, b3, b2, b1, b0))

#endif /* DT_BINDINGS_MACROS_H_ */
