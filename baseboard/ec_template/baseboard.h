/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ec_template baseboard configuration */

#ifndef __CROS_EC_BASEBOARD_H
#define __CROS_EC_BASEBOARD_H


/* EC Chipset Configuration */
#define NPCX7_PWM1_SEL    0  /* GPIO C2 is not used as PWM1. */
#define NPCX_UART_MODULE2 1  /* GPIO64/65 are used as UART pins. */
/* Internal SPI flash on NPCX796FC is 512 kB */
#define CONFIG_FLASH_SIZE (512 * 1024)
#define CONFIG_SPI_FLASH_REGS
#define CONFIG_SPI_FLASH_W25Q80 /* Internal SPI flash type. */
#define CONFIG_HIBERNATE_PSL

/* EC code features */
#define CONFIG_CRC8
#define CONFIG_LTO
#define CONFIG_VBOOT_HASH
#define CONFIG_VSTORE
#define CONFIG_VSTORE_SLOT_COUNT 1

/* AP to EC communication */

/* AP Chipset configuration */

/* I2C Buses */

/* CrOS Board Information */

/* Keyboard Configuration */

/* LEDs */

/* Sensors */

/* Power Sequencing */

/* Battery Charger */

/* Battery */

/* USB Type C */

/* USB PD */

/* USB PPC */

/* USB TCPC */

#ifndef __ASSEMBLER__

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BASEBOARD_H */
