/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file is included before chip_config.h.
 */

#ifndef __CROS_EC_BOARD_FLASH_H
#define __CROS_EC_BOARD_FLASH_H

/*#define CONFIG_VBOOT_EFS*/
#define CONFIG_FLASH_SIZE 0x80000

/*
 * Flash layout. Since config_flash_layout.h is included before board.h,
 * we can only overwrite (=undef/define) these parameters here.
 *
 * Flash stores 3 images: RO, RW_A, RW_B. We divide the flash by 4.
 * A public key is stored at the end of RO. Signatures are stored at the
 * end of RW_A and RW_B, respectively.
 */
#define CONFIG_RW_B
#define CONFIG_RW_B_MEM_OFF		CONFIG_RO_MEM_OFF
#undef  CONFIG_RO_SIZE
#define CONFIG_RO_SIZE			(CONFIG_FLASH_SIZE / 4)
#undef  CONFIG_RW_SIZE
#define CONFIG_RW_SIZE			(CONFIG_FLASH_SIZE*3 / 8)
#define CONFIG_RW_A_STORAGE_OFF		CONFIG_RW_STORAGE_OFF
#define CONFIG_RW_B_STORAGE_OFF		(CONFIG_RW_A_STORAGE_OFF + \
					 CONFIG_RW_SIZE)
#define CONFIG_RW_A_SIGN_STORAGE_OFF	(CONFIG_RW_A_STORAGE_OFF + \
					 CONFIG_RW_SIZE - CONFIG_RW_SIG_SIZE)
#define CONFIG_RW_B_SIGN_STORAGE_OFF	(CONFIG_RW_B_STORAGE_OFF + \
					 CONFIG_RW_SIZE - CONFIG_RW_SIG_SIZE)
#define CONFIG_RWSIG
#define CONFIG_RWSIG_TYPE_RWSIG
#define CONFIG_RSA
#ifdef SECTION_IS_RO
#define CONFIG_RSA_OPTIMIZED
#endif
#define CONFIG_SHA256
#ifdef SECTION_IS_RO
#define CONFIG_SHA256_UNROLLED
#endif
#define CONFIG_RSA_KEY_SIZE 3072
#define CONFIG_RSA_EXPONENT_3

#endif
