/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Ensure that we have a clean macro environment before including gpio.wrap.
 */
#pragma push_macro("UNUSED")
#pragma push_macro("UNIMPLEMENTED")
#undef UNSUED
#undef UNIMPLEMENTED