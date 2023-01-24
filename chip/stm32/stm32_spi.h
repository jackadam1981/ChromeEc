/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __CROS_EC_STM32_SPI_H
#define __CROS_EC_STM32_SPI_H

/*
 * Max data size for a version 3 request/response packet.  This is big enough
 * to handle a request/response header, flash write offset/size, and 512 bytes
 * of flash data.
 */
#define SPI_MAX_REQUEST_SIZE 0x220
#define SPI_MAX_RESPONSE_SIZE 0x220

#endif /* __CROS_EC_STM32_SPI_H */

