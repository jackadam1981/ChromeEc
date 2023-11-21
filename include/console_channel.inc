/* -*- mode:c -*-
 *
 * Copyright 2016 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

CONSOLE_CHANNEL(CC_COMMAND,      "command")
CONSOLE_CHANNEL(CC_ACCEL,        "accel")
#ifdef CONFIG_CEC
CONSOLE_CHANNEL(CC_CEC, "cec")
#endif
#ifdef CONFIG_EXTENSION_COMMAND
CONSOLE_CHANNEL(CC_EXTENSION,    "extension")
#endif
#if defined(CONFIG_PHYSICAL_PRESENCE)
CONSOLE_CHANNEL(CC_CCD,          "ccd")
#endif
CONSOLE_CHANNEL(CC_CHARGER,      "charger")
CONSOLE_CHANNEL(CC_CHIPSET,      "chipset")
CONSOLE_CHANNEL(CC_CLOCK,        "clock")
#ifdef CONFIG_DMA
CONSOLE_CHANNEL(CC_DMA,          "dma")
#endif
CONSOLE_CHANNEL(CC_EVENTS,       "events")
#ifdef HAS_TASK_FPSENSOR
CONSOLE_CHANNEL(CC_FP,           "fingerprint")
#endif
CONSOLE_CHANNEL(CC_GPIO,         "gpio")
#ifdef HAS_TASK_HOSTCMD
CONSOLE_CHANNEL(CC_HOSTCMD,      "hostcmd")
#endif
#ifdef CONFIG_I2C
CONSOLE_CHANNEL(CC_I2C,          "i2c")
#endif
#ifdef CONFIG_IPI
CONSOLE_CHANNEL(CC_IPI,          "ipi")
#endif
CONSOLE_CHANNEL(CC_KEYBOARD,     "keyboard")
CONSOLE_CHANNEL(CC_KEYSCAN,      "keyscan")
CONSOLE_CHANNEL(CC_LIDANGLE,     "lidangle")
/*
   Use CC_LPC for any host communication channels to x86,
   like LPC, eSPI, or IPC...
 */
#if defined(HAS_TASK_IPC_MNG) || defined(HAS_TASK_HECI_RX)
CONSOLE_CHANNEL(CC_LPC,          "lpc")
#endif
CONSOLE_CHANNEL(CC_MOTION_LID,   "motionlid")
CONSOLE_CHANNEL(CC_MOTION_SENSE, "motionsense")
#ifdef HAS_TASK_PDCMD
CONSOLE_CHANNEL(CC_PD_HOST_CMD,  "pdhostcm")
#endif
CONSOLE_CHANNEL(CC_PORT80,       "port80")
CONSOLE_CHANNEL(CC_SPI,          "spi")
#ifdef CONFIG_RBOX
CONSOLE_CHANNEL(CC_RBOX,         "rbox")
#endif
#ifdef CONFIG_SPP
CONSOLE_CHANNEL(CC_SPP,          "spp")
#endif
#if defined(CONFIG_SWITCH) || defined(CONFIG_LID_SWITCH)
CONSOLE_CHANNEL(CC_SWITCH,       "switch")
#endif
CONSOLE_CHANNEL(CC_SYSTEM,       "system")
CONSOLE_CHANNEL(CC_TASK,         "task")
#ifdef CONFIG_TOUCHPAD
CONSOLE_CHANNEL(CC_TOUCHPAD,     "touchpad")
#endif
#ifdef CONFIG_DPTF
CONSOLE_CHANNEL(CC_DPTF,         "dptf")
#endif
#ifdef CONFIG_ALS
CONSOLE_CHANNEL(CC_ALS,		 "als")
#endif
CONSOLE_CHANNEL(CC_THERMAL,      "thermal")
CONSOLE_CHANNEL(CC_TPM,          "tpm")
CONSOLE_CHANNEL(CC_USB,          "usb")
CONSOLE_CHANNEL(CC_USBCHARGE,    "usbcharge")
CONSOLE_CHANNEL(CC_VBOOT,        "vboot")
CONSOLE_CHANNEL(CC_HOOK,         "hook")
