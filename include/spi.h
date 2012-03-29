/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* SPI interface for Chrome EC */

#ifndef __CROS_EC_SPI_H
#define __CROS_EC_SPI_H

/* Initializes the module. */
extern int spi_init(int argc, char **argv);

extern int spi_read(int port, void *buf, int len);
extern int spi_write(int port, void *buf, int len);

#endif  /* __CROS_EC_SPI_H */
