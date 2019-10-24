/* Copyright (c) 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* BC-Link base register, 4000_CD00h / 4000_CD20h */
#define MEC17XX_BC_BASE(x)		  (0x4000CD00 + (x) * 0x20)

#define MEC17XX_BC_STATUS(x)		  REG32(MEC17XX_BC_BASE(x) + 0)
#define MEC17XX_BC_STATUS_RESET 	  (1 << 7)
#define MEC17XX_BC_STATUS_ERR 		  (1 << 6)
#define MEC17XX_BC_STATUS_ERR_INT_EN	  (1 << 5)
#define MEC17XX_BC_STATUS_BUSY_CLR_INT_EN (1 << 4)
#define MEC17XX_BC_STATUS_BUSY		  (1 << 0)

/* 7:0 -- Address in the compansion BC-Link transaction */
#define MEC17XX_BC_ADDR(x)		REG32(MEC17XX_BC_BASE(x) + 0x4)

/* 7:0 -- Data used in a BC-Link transactiojn */
#define MEC17XX_BC_DATA(x)		REG32(MEC17XX_BC_BASE(x) + 0x8)

/* 7:0 -- Clock divider */
#define MEC17XX_BC_CLK_SEL(x)		REG32(MEC17XX_BC_BASE(x) + 0xC)
