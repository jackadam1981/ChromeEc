/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ITE PD INTC control module */

#ifndef __CROS_EC_ITE_PD_INTC_H
#define __CROS_EC_ITE_PD_INTC_H

#ifdef CONFIG_USB_PD_TCPM_ITE_ON_CHIP
extern void chip_pd_irq(enum usbpd_port port);
#ifdef CONFIG_ZEPHYR
/* Use the Zephyr names here. When upstreaming we can update this */
#include <dt-bindings/interrupt-controller/ite-intc.h>

#define IT83XX_GPIO_GPCRF4	GPCRF4
#define IT83XX_GPIO_GPCRF5	GPCRF5
#define IT83XX_GPIO_GPCRH1	GPCRH1
#define IT83XX_GPIO_GPCRH2	GPCRH2
#define IT83XX_GPIO_GPCRP0	IT8XXX2_GPIO_GPCRP0
#define IT83XX_GPIO_GPCRP1	IT8XXX2_GPIO_GPCRP1
#define IT83XX_IRQ_USBPD0	IT8XXX2_IRQ_USBPD0
#define IT83XX_IRQ_USBPD1	IT8XXX2_IRQ_USBPD1
#define IT83XX_IRQ_USBPD2	IT8XXX2_IRQ_USBPD2
#define USB_VID_ITE		0x048d

/* ITE chip supports PD features */
#define IT83XX_INTC_FAST_SWAP_SUPPORT
#define IT83XX_INTC_PLUG_IN_OUT_SUPPORT
#endif
#endif

#endif /* __CROS_EC_ITE_PD_INTC_H */
