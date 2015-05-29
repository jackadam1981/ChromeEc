/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Helper macros for shared objects library.
 */
#ifndef __CROS_EC_LIBSHAREDOBJS_H
#define __CROS_EC_LIBSHAREDOBJS_H


#ifdef CONFIG_SHAREDLIB
/*
 * The shared library currently only works with those platforms in which both
 * the RO and RW images are stored together on the same flash chip and where the
 * RO image is executable from flash.
 */
#ifndef CONFIG_FLASH_IS_EXECUTABLE
#error "The shared library is NOT compatible with this EC."
#endif

/*
 * All of the objects in the shared library will be placed into the '.roshared'
 * section.  The SHAREDLIB() macro simply adds this attribute and prevents the
 * RW image from compiling them in.
 */
#undef SHAREDLIB
#ifdef RO_IMAGE
#define SHAREDLIB(...) __attribute__ ((section(".roshared"))) __VA_ARGS__
#else /* !defined(RO_IMAGE) */
#define SHAREDLIB(...)
#endif /* defined(RO_IMAGE) */

#else /* !defined(CONFIG_SHAREDLIB) */

/* By default, the SHAREDLIB() macro maps to its contents. */
#define SHAREDLIB(...) __VA_ARGS__
#endif /* defined(CONFIG_SHAREDLIB) */
#endif /* __CROS_EC_LIBSHAREDOBJS_H */
