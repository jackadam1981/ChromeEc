/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CHIP_TYPE_H_
#define CHIP_TYPE_H_

#if !defined(__C51__) && !defined(__CX51__)
#define code    const

#define xdata
#define bdata
#define idata
#endif

#ifndef NULL
#define NULL                        (0)
#endif

#ifndef FALSE
#define FALSE                       (0)
#endif

#ifndef TRUE
#define TRUE                        (1)
#endif

#define T_FALSE  FALSE
#define T_TRUE   TRUE

#define BIT(X) (1 << (X))

#define BIT0  0x00000001
#define BIT1  0x00000002
#define BIT2  0x00000004
#define BIT3  0x00000008
#define BIT4  0x00000010
#define BIT5  0x00000020
#define BIT6  0x00000040
#define BIT7  0x00000080

#define BIT8  0x00000100
#define BIT9  0x00000200
#define BIT10 0x00000400
#define BIT11 0x00000800
#define BIT12 0x00001000
#define BIT13 0x00002000
#define BIT14 0x00004000
#define BIT15 0x00008000

#define BIT16 0x00010000
#define BIT17 0x00020000
#define BIT18 0x00040000
#define BIT19 0x00080000
#define BIT20 0x00100000
#define BIT21 0x00200000
#define BIT22 0x00400000
#define BIT23 0x00800000

#define BIT24 0x01000000
#define BIT25 0x02000000
#define BIT26 0x04000000
#define BIT27 0x08000000
#define BIT28 0x10000000
#define BIT29 0x20000000
#define BIT30 0x40000000
#define BIT31 0x80000000

#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    1
#endif

#ifndef NULL
#define NULL (0)
#endif

#define MASK_bit0 (1 << 0)
#define MASK_bit1 (1 << 1)
#define MASK_bit2 (1 << 2)
#define MASK_bit3 (1 << 3)
#define MASK_bit4 (1 << 4)
#define MASK_bit5 (1 << 5)
#define MASK_bit6 (1 << 6)
#define MASK_bit7 (1 << 7)
#define CONCAT(a, b) a ## b
#define MASK(x) CONCAT(MASK_, x)

#define ECReg(x) (*((volatile unsigned char *)(x)))
#define ECReg16(x) (*((volatile unsigned short *)(x)))
#define ECReg32(x) (*((volatile unsigned long *)(x)))

#define SD_uchar_8(x) (*((unsigned char *)(x)))
#define SD_uint_16(x) (*((unsigned short *)(x)))
#define SD_ulong_32(x) (*((unsigned long *)(x)))

#define SD_Ptr_uchar_8(x) ((unsigned char *)(x))
#define SD_Ptr_uint_16(x) ((unsigned short *)(x))
#define SD_Ptr_ulong_32(x) ((unsigned long *)(x))

#define SET_MASK(reg, bit_mask)      ((reg) |= (bit_mask))
#define CLEAR_MASK(reg, bit_mask)    ((reg) &= (~(bit_mask)))
#define IS_MASK_SET(reg, bit_mask)   (((reg) & (bit_mask)) != 0)
#define IS_MASK_CLEAR(reg, bit_mask) (((reg) & (bit_mask)) == 0)
#define INVERSE_REG(reg, bit)           ((reg) ^= (0x1 << (bit)))
#define INVERSE_REG_MASK(reg, bit_mask) ((reg) ^= (bit_mask))


#define Word_Mask(x)        (1 << (x))
#define Cpl_Byte_Mask(x)    (~(1 << (x)))
#define Cpl_Word_Mask(x)    (~(1 << (x)))

#define COUNT_OF_ARRAY(p)   (sizeof(p)/sizeof(p[0]))
#define FIELD   unsigned char
#endif /* CHIP_TYPE_H_ */

