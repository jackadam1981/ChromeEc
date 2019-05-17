/* -*- mode: c -*-
 *
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Reset flag definitions. This file is sourced multiple times with a
 * redefinition of RESET_FLAG to create the correct constants (similar
 * pattern to EC tasklist, or console channels).
 */

/* Other (known) reason */
RESET_FLAG(RESET_FLAG_OTHER, "other")

/* Reset pin asserted */
RESET_FLAG(RESET_FLAG_RESET_PIN, "reset-pin")

/* Brownout */
RESET_FLAG(RESET_FLAG_BROWNOUT, "brownout")

/* Power-on reset */
RESET_FLAG(RESET_FLAG_POWER_ON, "power-on")

/* Watchdog timer reset */
RESET_FLAG(RESET_FLAG_WATCHDOG, "watchdog")

/* Soft reset triggered by core */
RESET_FLAG(RESET_FLAG_SOFT, "soft")

/* Wake from hibernate */
RESET_FLAG(RESET_FLAG_HIBERNATE, "hibernate")

/* RTC alarm wake */
RESET_FLAG(RESET_FLAG_RTC_ALARM, "rtc-alarm")

/* Wake-pin triggered wake */
RESET_FLAG(RESET_FLAG_WAKE_PIN, "wake-pin")

/* Low battery triggered wake */
RESET_FLAG(RESET_FLAG_LOW_BATTERY, "low-battery")

/* Jumped directly to this image */
RESET_FLAG(RESET_FLAG_SYSJUMP, "sysjump")

/* Hard reset from software */
RESET_FLAG(RESET_FLAG_HARD, "hard")

/* Do not power on AP */
RESET_FLAG(RESET_FLAG_AP_OFF, "ap-off")

/* Reset flags preserved from previous boot */
RESET_FLAG(RESET_FLAG_PRESERVED, "preserved")

/* USB resume triggered wake */
RESET_FLAG(RESET_FLAG_USB_RESUME, "usb-resume")

/* USB Type-C debug cable */
RESET_FLAG(RESET_FLAG_RDD, "rdd")

/* Fixed reset functionality */
RESET_FLAG(RESET_FLAG_RBOX, "rbox")

/* Security threat */
RESET_FLAG(RESET_FLAG_SECURITY, "security")

/* AP experienced a watchdog reset */
RESET_FLAG(RESET_FLAG_AP_WATCHDOG, "ap-watchdog")
