/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Register map for NUCMX processor
 */

#ifndef __CROS_EC_REGISTERS_H
#define __CROS_EC_REGISTERS_H

#include "common.h"
/*******************************************************************************************/
/*------------------------------------------------------------------------------------------
 * 									Macro Functions
 *-----------------------------------------------------------------------------------------*/
#define SET_BIT(reg, bit)       		((reg)|=(0x1<<(bit)))
#define CLEAR_BIT(reg, bit)     		((reg)&=(~(0x1<<(bit))))
#define IS_BIT_SET(reg, bit)    		((reg>>bit)&(0x1))

/*------------------------------------------------------------------------------------------
 *							NUCMX Register Definitions
 *-----------------------------------------------------------------------------------------*/
/* Global Definition */
#define	NPCX5M5G						1
#define I2C0_BUS0						1	/* Use I2C0_SDA0/1 I2C0_SCL0/1 */
#define TACH_SEL1						1	/* Use TACH_SEL1 or TACH_SEL2 */
#define JTAG1							0 	/* Use JTAG0 or 1 only support 132-Pins */
#define I2C_7BITS_ADDR					0
#define I2C_LEVEL_SUPPORT				1
/* Switcher of features */
#define SUPPORT_LCT						1
#define SUPPORT_WDG						0
#define SUPPORT_HIB						1
#define SUPPORT_JTAG					1
/* Switcher of debugging */
#define DEBUG_I2C						0
#define DEBUG_TMR						1
#define DEBUG_WDG						0
#define DEBUG_GPIO						1
#define DEBUG_FAN						0
#define DEBUG_PWM						0
#define DEBUG_SPI						0
#define DEBUG_FLH						0
#define DEBUG_CLK						1

/* Modules Map */
#define NUCMX_MDC_BASE_ADDR				0x4000C000
#define NUCMX_SIB_BASE_ADDR				0x4000E000
#define NUCMX_PMC_BASE_ADDR				0x4000D000
#define NUCMX_SHM_BASE_ADDR				0x40010000
#define NUCMX_FIU_BASE_ADDR				0x40020000
#define NUCMX_KBSCAN_REGS_BASE          0x400A3000
#define NUCMX_GLUE_REGS_BASE          	0x400A5000
#define NUCMX_BBRAM_BASE_ADDR			0x400AF000
#define NUCMX_HFCG_BASE_ADDR			0x400B5000
#define NUCMX_MTC_BASE_ADDR				0x400B7000
#define NUCMX_MSWC_BASE_ADDR			0x400C1000
#define NUCMX_SCFG_BASE_ADDR            0x400C3000
#define NUCMX_CR_UART_BASE_ADDR			0x400C4000
#define NUCMX_KBC_BASE_ADDR				0x400C7000
#define NUCMX_ADC_BASE_ADDR             0x400D1000
#define NUCMX_SPI_BASE_ADDR             0x400D2000
#define NUCMX_PECI_BASE_ADDR            0x400D4000
#define NUCMX_TWD_BASE_ADDR				0x400D8000

/* Multi-Modules Map */
#define NUCMX_PWM_BASE_ADDR(mdl)     	(0x40080000 + ((mdl) * 0x2000L))
#define NUCMX_GPIO_BASE_ADDR(mdl)      	(0x40081000 + ((mdl) * 0x2000L))
#define NUCMX_ITIM16_BASE_ADDR(mdl)  	(0x400B0000 + ((mdl) * 0x2000L))
#define NUCMX_MIWU_BASE_ADDR(mdl)    	(0x400BB000 + ((mdl) * 0x2000L))
#define NUCMX_MFT_BASE_ADDR(mdl)		(0x400E1000 + ((mdl) * 0x2000L))
#define NUCMX_PM_CH_BASE_ADDR(mdl)		(0x400C9000 + ((mdl) * 0x2000L))
#define NUCMX_SMB_BASE_ADDR(mdl)     	((mdl<2) ? (0x40009000 + ((mdl) * 0x2000L)) : \
		                                 (0x400C0000 + ((mdl) * 0x2000L)))

/*
 * NUCMX-IRQ numbers
 */
#define NUCMX_IRQ_0						0
#define NUCMX_IRQ_1						1
#define NUCMX_IRQ_2                     2
#define NUCMX_IRQ_3                     3
#define NUCMX_IRQ_4                     4
#define NUCMX_IRQ_5                     5
#define NUCMX_IRQ_6                     6
#define NUCMX_IRQ_7                     7
#define NUCMX_IRQ_8                     8
#define NUCMX_IRQ_9                     9
#define NUCMX_IRQ_10                    10
#define NUCMX_IRQ_11                    11
#define NUCMX_IRQ_12                    12
#define NUCMX_IRQ_13                    13
#define NUCMX_IRQ_14                    14
#define NUCMX_IRQ_15                    15
#define NUCMX_IRQ_16                    16
#define NUCMX_IRQ_17                    17
#define NUCMX_IRQ_18                    18
#define NUCMX_IRQ_19                    19
#define NUCMX_IRQ_20                    20
#define NUCMX_IRQ_21                    21
#define NUCMX_IRQ_22                    22
#define NUCMX_IRQ_23                    23
#define NUCMX_IRQ_24                    24
#define NUCMX_IRQ_25                    25
#define NUCMX_IRQ_26                    26
#define NUCMX_IRQ_27                    27
#define NUCMX_IRQ_28                    28
#define NUCMX_IRQ_29                    29
#define NUCMX_IRQ_30                    30
#define NUCMX_IRQ_31                    31
#define NUCMX_IRQ_32                    32
#define NUCMX_IRQ_33                    33
#define NUCMX_IRQ_34                    34
#define NUCMX_IRQ_35                    35
#define NUCMX_IRQ_36                    36
#define NUCMX_IRQ_37                    37
#define NUCMX_IRQ_38                    38
#define NUCMX_IRQ_39                    39
#define NUCMX_IRQ_40                    40
#define NUCMX_IRQ_41                    41
#define NUCMX_IRQ_42                    42
#define NUCMX_IRQ_43                    43
#define NUCMX_IRQ_44                    44
#define NUCMX_IRQ_45                    45
#define NUCMX_IRQ_46                    46
#define NUCMX_IRQ_47                    47
#define NUCMX_IRQ_48                    48
#define NUCMX_IRQ_49                    49
#define NUCMX_IRQ_50                    50
#define NUCMX_IRQ_51                    51
#define NUCMX_IRQ_52                    52
#define NUCMX_IRQ_53                    53
#define NUCMX_IRQ_54                    54
#define NUCMX_IRQ_55                    55
#define NUCMX_IRQ_56                    56
#define NUCMX_IRQ_57                    57
#define NUCMX_IRQ_58                    58
#define NUCMX_IRQ_59                    59
#define NUCMX_IRQ_60                    60
#define NUCMX_IRQ_61                    61
#define NUCMX_IRQ_62                    62
#define NUCMX_IRQ_63                    63

#define NUCMX_IRQ0_NOUSED				NUCMX_IRQ_0
#define NUCMX_IRQ1_NOUSED				NUCMX_IRQ_1
#define NUCMX_IRQ_KBSCAN               	NUCMX_IRQ_2
#define NUCMX_IRQ_PM_CHAN_OBF           NUCMX_IRQ_3
#define NUCMX_IRQ_PECI                  NUCMX_IRQ_4
#define NUCMX_IRQ5_NOUSED			    NUCMX_IRQ_5
#define NUCMX_IRQ_PORT80                NUCMX_IRQ_6
#define NUCMX_IRQ_MTC_WKINTAD_0			NUCMX_IRQ_7
#define NUCMX_IRQ8_NOUSED			    NUCMX_IRQ_8
#define NUCMX_IRQ_MFT_1			        NUCMX_IRQ_9
#define NUCMX_IRQ_ADC                   NUCMX_IRQ_10
#define NUCMX_IRQ_WKINTEFGH_0	    	NUCMX_IRQ_11
#define NUCMX_IRQ_CDMA			        NUCMX_IRQ_12
#define NUCMX_IRQ_SMB1			        NUCMX_IRQ_13
#define NUCMX_IRQ_SMB2			        NUCMX_IRQ_14
#define NUCMX_IRQ_WKINTC_0          	NUCMX_IRQ_15
#define NUCMX_IRQ16_NOUSED      		NUCMX_IRQ_16
#define NUCMX_IRQ_ITIM16_3              NUCMX_IRQ_17
#define NUCMX_IRQ_ESPI					NUCMX_IRQ_18
#define NUCMX_IRQ19_NOUSED				NUCMX_IRQ_19
#define NUCMX_IRQ20_NOUSED				NUCMX_IRQ_20
#define NUCMX_IRQ_PS2                   NUCMX_IRQ_21
#define NUCMX_IRQ22_NOUSED				NUCMX_IRQ_22
#define NUCMX_IRQ_MFT_2			        NUCMX_IRQ_23
#define NUCMX_IRQ_SHM                   NUCMX_IRQ_24
#define NUCMX_IRQ_KBC_IBF               NUCMX_IRQ_25
#define NUCMX_IRQ_PM_CHAN_IBF           NUCMX_IRQ_26
#define NUCMX_IRQ_ITIM16_2              NUCMX_IRQ_27
#define NUCMX_IRQ_ITIM16_1              NUCMX_IRQ_28
#define NUCMX_IRQ29_NOUSED				NUCMX_IRQ_29
#define NUCMX_IRQ30_NOUSED				NUCMX_IRQ_30
#define NUCMX_IRQ_TWD_WKINTB_0          NUCMX_IRQ_31
#define NUCMX_IRQ32_NOUSED				NUCMX_IRQ_32
#define NUCMX_IRQ_UART			        NUCMX_IRQ_33
#define NUCMX_IRQ34_NOUSED				NUCMX_IRQ_34
#define NUCMX_IRQ35_NOUSED				NUCMX_IRQ_35
#define NUCMX_IRQ_SMB3			        NUCMX_IRQ_36
#define NUCMX_IRQ_SMB4			        NUCMX_IRQ_37
#define NUCMX_IRQ38_NOUSED				NUCMX_IRQ_38
#define NUCMX_IRQ39_NOUSED				NUCMX_IRQ_39
#define NUCMX_IRQ40_NOUSED				NUCMX_IRQ_40
#define NUCMX_IRQ_MFT_3			        NUCMX_IRQ_41
#define NUCMX_IRQ42_NOUSED				NUCMX_IRQ_42
#define NUCMX_IRQ_ITIM16_4			    NUCMX_IRQ_43
#define NUCMX_IRQ_ITIM16_5			    NUCMX_IRQ_44
#define NUCMX_IRQ_ITIM16_6			    NUCMX_IRQ_45
#define NUCMX_IRQ46_NOUSED				NUCMX_IRQ_46
#define NUCMX_IRQ_WKINTA_1              NUCMX_IRQ_47
#define NUCMX_IRQ_WKINTB_1              NUCMX_IRQ_48
#define NUCMX_IRQ_KSI_WKINTC_1          NUCMX_IRQ_49
#define NUCMX_IRQ_WKINTD_1              NUCMX_IRQ_50
#define NUCMX_IRQ_WKINTE_1              NUCMX_IRQ_51
#define NUCMX_IRQ_WKINTF_1              NUCMX_IRQ_52
#define NUCMX_IRQ_WKINTG_1              NUCMX_IRQ_53
#define NUCMX_IRQ_WKINTH_1              NUCMX_IRQ_54
#define NUCMX_IRQ55_NOUSED				NUCMX_IRQ_55
#define NUCMX_IRQ_KBC_OBF               NUCMX_IRQ_56
#define NUCMX_IRQ_SPI               	NUCMX_IRQ_57
#define NUCMX_IRQ58_NOUSED				NUCMX_IRQ_58
#define NUCMX_IRQ59_NOUSED				NUCMX_IRQ_59
#define NUCMX_IRQ_WKINTA_2              NUCMX_IRQ_60
#define NUCMX_IRQ_WKINTB_2              NUCMX_IRQ_61
#define NUCMX_IRQ_WKINTC_2              NUCMX_IRQ_62
#define NUCMX_IRQ_WKINTD_2              NUCMX_IRQ_63

#define NUCMX_IRQ_COUNT          		64

/*******************************************************************************************/
/* High Frequency Clock Generator (HFCG) registers */
#define NUCMX_HFCGCTRL                	REG8 (NUCMX_HFCG_BASE_ADDR + 0x000)
#define NUCMX_HFCGML                  	REG8 (NUCMX_HFCG_BASE_ADDR + 0x002)
#define NUCMX_HFCGMH                  	REG8 (NUCMX_HFCG_BASE_ADDR + 0x004)
#define NUCMX_HFCGN                   	REG8 (NUCMX_HFCG_BASE_ADDR + 0x006)
#define NUCMX_HFCGP                   	REG8 (NUCMX_HFCG_BASE_ADDR + 0x008)
#define NUCMX_HFCBCD                  	REG8 (NUCMX_HFCG_BASE_ADDR + 0x010)

/* HFCG register fields */
#define NUCMX_HFCGCTRL_LOAD           	0
#define NUCMX_HFCGCTRL_LOCK           	2
#define NUCMX_HFCGCTRL_CLK_CHNG       	7

/*******************************************************************************************/
/*CR UART Register */
#define NUCMX_UTBUF                    	REG8 (NUCMX_CR_UART_BASE_ADDR + 0x000)
#define NUCMX_URBUF                    	REG8 (NUCMX_CR_UART_BASE_ADDR + 0x002)
#define NUCMX_UICTRL                    REG8 (NUCMX_CR_UART_BASE_ADDR + 0x004)
#define NUCMX_USTAT                    	REG8 (NUCMX_CR_UART_BASE_ADDR + 0x006)
#define NUCMX_UFRS                    	REG8 (NUCMX_CR_UART_BASE_ADDR + 0x008)
#define NUCMX_UMDSL                    	REG8 (NUCMX_CR_UART_BASE_ADDR + 0x00A)
#define NUCMX_UBAUD                    	REG8 (NUCMX_CR_UART_BASE_ADDR + 0x00C)
#define NUCMX_UPSR                    	REG8 (NUCMX_CR_UART_BASE_ADDR + 0x00E)

/*******************************************************************************************/
/* KBSCAN registers */
#define NUCMX_KBSIN          			REG8 (NUCMX_KBSCAN_REGS_BASE + 0x04)
#define NUCMX_KBSINPU        			REG8 (NUCMX_KBSCAN_REGS_BASE + 0x05)
#define NUCMX_KBSOUT0        			REG16(NUCMX_KBSCAN_REGS_BASE + 0x06)
#define NUCMX_KBSOUT1        			REG16(NUCMX_KBSCAN_REGS_BASE + 0x08)
#define NUCMX_KBS_BUF_INDX   			REG8 (NUCMX_KBSCAN_REGS_BASE + 0x0A)
#define NUCMX_KBS_BUF_DATA   			REG8 (NUCMX_KBSCAN_REGS_BASE + 0x0B)
#define NUCMX_KBSEVT         			REG8 (NUCMX_KBSCAN_REGS_BASE + 0x0C)
#define NUCMX_KBSCTL         			REG8 (NUCMX_KBSCAN_REGS_BASE + 0x0D)
#define NUCMX_KBS_CFG_INDX   			REG8 (NUCMX_KBSCAN_REGS_BASE + 0x0E)
#define NUCMX_KBS_CFG_DATA   			REG8 (NUCMX_KBSCAN_REGS_BASE + 0x0F)

/* KBSCAN register fields */
#define NUCMX_KBSBUFINDX           		0
#define NUCMX_KBSDONE            		0
#define NUCMX_KBSERR            		1
#define NUCMX_KBSSTART            		0
#define NUCMX_KBSMODE            		1
#define NUCMX_KBSIEN            		2
#define NUCMX_KBSINC            		3
#define NUCMX_KBSCFGINDX           		0

/* KBSCAN definitions */
#define KB_ROW_NUM  					8						/* Rows numbers of keyboard matrix		*/
#define KB_COL_NUM      				18						/* Columns numbers of keyboard matrix	*/
#define KB_ROW_MASK     				((1<<KB_ROW_NUM) -1)	/* Mask of rows of keyboard matrix		*/
#define KB_COL_MASK     				((1<<KB_COL_NUM) -1)	/* Mask of columns of keyboard matrix	*/

/*******************************************************************************************/
/* GLUE registers */
#define NUCMX_GLUE_SDPD0				REG8 (NUCMX_GLUE_REGS_BASE + 0x010)
#define NUCMX_GLUE_SDPD1				REG8 (NUCMX_GLUE_REGS_BASE + 0x012)
#define NUCMX_GLUE_SDP_CTS				REG8 (NUCMX_GLUE_REGS_BASE + 0x014)

/*******************************************************************************************/
/* MIWU registers */
#define NUCMX_WKEDG_ADDR(port,n)        (NUCMX_MIWU_BASE_ADDR(port) + 0x00 + ((n) * 2L) + ((n) < 5 ? 0 : 0x1E))
#define NUCMX_WKAEDG_ADDR(port,n)       (NUCMX_MIWU_BASE_ADDR(port) + 0x01 + ((n) * 2L) + ((n) < 5 ? 0 : 0x1E))
#define NUCMX_WKPND_ADDR(port,n)        (NUCMX_MIWU_BASE_ADDR(port) + 0x0A + ((n) * 4L) + ((n) < 5 ? 0 : 0x10))
#define NUCMX_WKPCL_ADDR(port,n)        (NUCMX_MIWU_BASE_ADDR(port) + 0x0C + ((n) * 4L) + ((n) < 5 ? 0 : 0x10))
#define NUCMX_WKEN_ADDR(port,n)         (NUCMX_MIWU_BASE_ADDR(port) + 0x1E + ((n) * 2L) + ((n) < 5 ? 0 : 0x12))
#define NUCMX_WKMOD_ADDR(port,n)        (NUCMX_MIWU_BASE_ADDR(port) + 0x70 + n)

#define NUCMX_WKEDG(port,n)             REG8 (NUCMX_WKEDG_ADDR(port,n))
#define NUCMX_WKAEDG(port,n)            REG8 (NUCMX_WKAEDG_ADDR(port,n))
#define NUCMX_WKPND(port,n)             REG8 (NUCMX_WKPND_ADDR(port,n))
#define NUCMX_WKPCL(port,n)             REG8 (NUCMX_WKPCL_ADDR(port,n))
#define NUCMX_WKEN(port,n)              REG8 (NUCMX_WKEN_ADDR(port,n))
#define NUCMX_WKMOD(port,n)				REG8 (NUCMX_WKMOD_ADDR(port,n))

/* MIWU enumeration */
enum {
	MIWU_TABLE_0,
	MIWU_TABLE_1,
	MIWU_TABLE_2,
	MIWU_TABLE_COUNT
};

enum {
	MIWU_GROUP_1,
	MIWU_GROUP_2,
	MIWU_GROUP_3,
	MIWU_GROUP_4,
	MIWU_GROUP_5,
	MIWU_GROUP_6,
	MIWU_GROUP_7,
	MIWU_GROUP_8,
	MIWU_GROUP_COUNT
};

enum {
	MIWU_EDGE_RISING,
	MIWU_EDGE_FALLING,
};

/* MIWU utilities */
#define MIWU_TABLE_WKKEY MIWU_TABLE_1
#define MIWU_GROUP_WKKEY MIWU_GROUP_3

/*******************************************************************************************/
/* GPIO registers */
#define NUCMX_PDOUT(n)                	REG8 (NUCMX_GPIO_BASE_ADDR(n) + 0x000)
#define NUCMX_PDIN(n)                 	REG8 (NUCMX_GPIO_BASE_ADDR(n) + 0x001)
#define NUCMX_PDIR(n)                 	REG8 (NUCMX_GPIO_BASE_ADDR(n) + 0x002)
#define NUCMX_PPULL(n)                	REG8 (NUCMX_GPIO_BASE_ADDR(n) + 0x003)
#define NUCMX_PPUD(n)                 	REG8 (NUCMX_GPIO_BASE_ADDR(n) + 0x004)
#define NUCMX_PENVDD(n)               	REG8 (NUCMX_GPIO_BASE_ADDR(n) + 0x005)
#define NUCMX_PTYPE(n)                	REG8 (NUCMX_GPIO_BASE_ADDR(n) + 0x006)

/* GPIO enumeration */
enum {
	GPIO_PORT_0,
	GPIO_PORT_1,
	GPIO_PORT_2,
	GPIO_PORT_3,
	GPIO_PORT_4,
	GPIO_PORT_5,
	GPIO_PORT_6,
	GPIO_PORT_7,
	GPIO_PORT_8,
	GPIO_PORT_9,
	GPIO_PORT_A,
	GPIO_PORT_B,
	GPIO_PORT_C,
	GPIO_PORT_D,
	GPIO_PORT_E,
	GPIO_PORT_F,
	GPIO_PORT_COUNT
};

enum {
	MASK_PIN0 = (1<<0),
	MASK_PIN1 = (1<<1),
	MASK_PIN2 = (1<<2),
	MASK_PIN3 = (1<<3),
	MASK_PIN4 = (1<<4),
	MASK_PIN5 = (1<<5),
	MASK_PIN6 = (1<<6),
	MASK_PIN7 = (1<<7),
};

/* Chip-independent aliases for port base group */
#define GPIO_0 GPIO_PORT_0
#define GPIO_1 GPIO_PORT_1
#define GPIO_2 GPIO_PORT_2
#define GPIO_3 GPIO_PORT_3
#define GPIO_4 GPIO_PORT_4
#define GPIO_5 GPIO_PORT_5
#define GPIO_6 GPIO_PORT_6
#define GPIO_7 GPIO_PORT_7
#define GPIO_8 GPIO_PORT_8
#define GPIO_9 GPIO_PORT_9
#define GPIO_A GPIO_PORT_A
#define GPIO_B GPIO_PORT_B
#define GPIO_C GPIO_PORT_C
#define GPIO_D GPIO_PORT_D
#define GPIO_E GPIO_PORT_E
#define GPIO_F GPIO_PORT_F

/*******************************************************************************************/
/* MSWC Registers */
#define NUCMX_MSWCTL1                  	REG8 (NUCMX_MSWC_BASE_ADDR + 0x000)
#define NUCMX_HCBAL                  	REG8 (NUCMX_MSWC_BASE_ADDR + 0x008)
#define NUCMX_HCBAH                  	REG8 (NUCMX_MSWC_BASE_ADDR + 0x00A)

/*******************************************************************************************/
/* System Configuration (SCFG) Registers */
#define NUCMX_DEVCNT                  	REG8 (NUCMX_SCFG_BASE_ADDR + 0x000)
#define NUCMX_STRPST                  	REG8 (NUCMX_SCFG_BASE_ADDR + 0x001)
#define NUCMX_RSTCTL                  	REG8 (NUCMX_SCFG_BASE_ADDR + 0x002)
#define NUCMX_DEV_CTL4                	REG8 (NUCMX_SCFG_BASE_ADDR + 0x006)
#define NUCMX_DEVALT(n)                 REG8 (NUCMX_SCFG_BASE_ADDR + 0x010 + n)
#define NUCMX_DEVPU0                  	REG8 (NUCMX_SCFG_BASE_ADDR + 0x028)
#define NUCMX_DEVPU1                  	REG8 (NUCMX_SCFG_BASE_ADDR + 0x029)
#define NUCMX_LV_GPIO_CTL0              REG8 (NUCMX_SCFG_BASE_ADDR + 0x02A)
#define NUCMX_LV_GPIO_CTL1              REG8 (NUCMX_SCFG_BASE_ADDR + 0x02B)
#define NUCMX_LV_GPIO_CTL2              REG8 (NUCMX_SCFG_BASE_ADDR + 0x02C)
#define NUCMX_LV_GPIO_CTL3              REG8 (NUCMX_SCFG_BASE_ADDR + 0x02D)
#define NUCMX_SCFG_VER                	REG8 (NUCMX_SCFG_BASE_ADDR + 0x02F)

#define TEST_BKSL                		REG8 (NUCMX_SCFG_BASE_ADDR + 0x037)
#define TEST0                			REG8 (NUCMX_SCFG_BASE_ADDR + 0x038)
#define BLKSEL							0

/* SCFG enumeration */
enum {
	ALT_GROUP_0,
	ALT_GROUP_1,
	ALT_GROUP_2,
	ALT_GROUP_3,
	ALT_GROUP_4,
	ALT_GROUP_5,
	ALT_GROUP_6,
	ALT_GROUP_7,
	ALT_GROUP_8,
	ALT_GROUP_9,
	ALT_GROUP_A,
	ALT_GROUP_B,
	ALT_GROUP_C,
	ALT_GROUP_D,
	ALT_GROUP_E,
	ALT_GROUP_F,
	ALT_GROUP_COUNT
};

/* SCFG register fields */
#define NUCMX_DEVCNT_F_SPI_TRIS       	6
#define NUCMX_DEVCNT_JEN1_HEN      		5
#define NUCMX_DEVCNT_JEN0_HEN       	4
#define NUCMX_STRPST_TRIST            	1
#define NUCMX_STRPST_TEST            	2
#define NUCMX_STRPST_JEN1             	4
#define NUCMX_STRPST_JEN0             	5
#define NUCMX_STRPST_SPI_COMP           7
#define NUCMX_RSTCTL_VCC1_RST_STS		0
#define NUCMX_RSTCTL_DBGRST_STS       	1
#define NUCMX_RSTCTL_LRESET_PLTRST_MODE	5
#define NUCMX_RSTCTL_HIPRST_MODE		6
#define NUCMX_DEV_CTL4_SPI_SP_SEL		4
#define NUCMX_DEVPU0_I2C0_0_PUE         0
#define NUCMX_DEVPU0_I2C0_1_PUE         1
#define NUCMX_DEVPU0_I2C1_0_PUE         2
#define NUCMX_DEVPU0_I2C2_0_PUE         4
#define NUCMX_DEVPU0_I2C3_0_PUE         6
#define NUCMX_DEVPU1_F_SPI_PUD_EN     	7

/* DEVALT */
#define NUCMX_DEVALT0_SPIP_SL			0
#define NUCMX_DEVALT0_GPIO_NO_SPIP		3
#define NUCMX_DEVALT0_F_SPI_CS1_2		4
#define NUCMX_DEVALT0_F_SPI_CS1_1		5
#define NUCMX_DEVALT0_F_SPI_QUAD		6
#define NUCMX_DEVALT0_NO_F_SPI			7

#define NUCMX_DEVALT1_KBRST_SL			0
#define NUCMX_DEVALT1_A20M_SL			1
#define NUCMX_DEVALT1_SMI_SL			2
#define NUCMX_DEVALT1_EC_SCI_SL			3
#define NUCMX_DEVALT1_NO_PWRGD			4
#define NUCMX_DEVALT1_RST_OUT_SL		5
#define NUCMX_DEVALT1_CLKRN_SL			6
#define NUCMX_DEVALT1_NO_LPC_ESPI		7

#define NUCMX_DEVALT2_I2C0_0_SL			0
#define NUCMX_DEVALT2_I2C0_1_SL			1
#define NUCMX_DEVALT2_I2C1_0_SL			2
#define NUCMX_DEVALT2_I2C2_0_SL			4
#define NUCMX_DEVALT2_I2C3_0_SL			6

#define NUCMX_DEVALT3_PS2_0_SL			0
#define NUCMX_DEVALT3_PS2_1_SL			1
#define NUCMX_DEVALT3_PS2_2_SL			2
#define NUCMX_DEVALT3_PS2_3_SL			3
#define NUCMX_DEVALT3_TA1_TACH1_SL1		4
#define NUCMX_DEVALT3_TB1_TACH2_SL1		5
#define NUCMX_DEVALT3_TA2_SL1			6
#define NUCMX_DEVALT3_TB2_SL1			7

#define NUCMX_DEVALT4_PWM0_SL			0
#define NUCMX_DEVALT4_PWM1_SL			1
#define NUCMX_DEVALT4_PWM2_SL			2
#define NUCMX_DEVALT4_PWM3_SL			3
#define NUCMX_DEVALT4_PWM4_SL			4
#define NUCMX_DEVALT4_PWM5_SL			5
#define NUCMX_DEVALT4_PWM6_SL			6
#define NUCMX_DEVALT4_PWM7_SL			7

#define NUCMX_DEVALT5_TRACE_EN        	0
#define NUCMX_DEVALT5_NJEN1_EN        	1
#define NUCMX_DEVALT5_NJEN0_EN         	2

#define NUCMX_DEVALT6_ADC0_SL			0
#define NUCMX_DEVALT6_ADC1_SL			1
#define NUCMX_DEVALT6_ADC2_SL			2
#define NUCMX_DEVALT6_ADC3_SL			3
#define NUCMX_DEVALT6_ADC4_SL			4

#define NUCMX_DEVALT7_NO_KSI0_SL		0
#define NUCMX_DEVALT7_NO_KSI1_SL		1
#define NUCMX_DEVALT7_NO_KSI2_SL		2
#define NUCMX_DEVALT7_NO_KSI3_SL		3
#define NUCMX_DEVALT7_NO_KSI4_SL		4
#define NUCMX_DEVALT7_NO_KSI5_SL		5
#define NUCMX_DEVALT7_NO_KSI6_SL		6
#define NUCMX_DEVALT7_NO_KSI7_SL		7

#define NUCMX_DEVALT8_NO_KSO00_SL		0
#define NUCMX_DEVALT8_NO_KSO01_SL		1
#define NUCMX_DEVALT8_NO_KSO02_SL		2
#define NUCMX_DEVALT8_NO_KSO03_SL		3
#define NUCMX_DEVALT8_NO_KSO04_SL		4
#define NUCMX_DEVALT8_NO_KSO05_SL		5
#define NUCMX_DEVALT8_NO_KSO06_SL		6
#define NUCMX_DEVALT8_NO_KSO07_SL		7

#define NUCMX_DEVALT9_NO_KSO08_SL		0
#define NUCMX_DEVALT9_NO_KSO09_SL		1
#define NUCMX_DEVALT9_NO_KSO10_SL		2
#define NUCMX_DEVALT9_NO_KSO11_SL		3
#define NUCMX_DEVALT9_NO_KSO12_SL		4
#define NUCMX_DEVALT9_NO_KSO13_SL		5
#define NUCMX_DEVALT9_NO_KSO14_SL		6
#define NUCMX_DEVALT9_NO_KSO15_SL		7

#define NUCMX_DEVALTA_NO_KSO16_SL		0
#define NUCMX_DEVALTA_NO_KSO17_SL		1
#define NUCMX_DEVALTA_32K_OUT_SL		2
#define NUCMX_DEVALTA_32KCLKIN_SL		3
#define NUCMX_DEVALTA_NO_VCC1_RST		4
#define NUCMX_DEVALTA_NO_PECI_EN		6
#define NUCMX_DEVALTA_UART_SL			7

#define NUCMX_DEVALTB_RXD_SL			0
#define NUCMX_DEVALTB_TXD_SL			1

#define NUCMX_DEVALTC_PS2_3_SL2			3
#define NUCMX_DEVALTC_TA1_TACH1_SL2		4
#define NUCMX_DEVALTC_TB1_TACH2_SL2		5
#define NUCMX_DEVALTC_TA2_SL2			6
#define NUCMX_DEVALTC_TB2_SL2			7

/*******************************************************************************************/
/* Development and Debug Support (DBG) Registers */
#define NUCMX_DBGCTRL                  	REG8 (NUCMX_SCFG_BASE_ADDR + 0x074)
#define NUCMX_DBGFRZEN1                	REG8 (NUCMX_SCFG_BASE_ADDR + 0x076)
#define NUCMX_DBGFRZEN2                	REG8 (NUCMX_SCFG_BASE_ADDR + 0x077)
#define NUCMX_DBGFRZEN3                	REG8 (NUCMX_SCFG_BASE_ADDR + 0x078)
/* DBG register fields */
#define NUCMX_DBGFRZEN3_GLBL_FRZ_DIS	7


/*******************************************************************************************/
/* SMBus Registers */
#define NUCMX_SMBSDA(n)               	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x000)
#define NUCMX_SMBST(n)                	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x002)
#define NUCMX_SMBCST(n)               	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x004)
#define NUCMX_SMBCTL1(n)              	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x006)
#define NUCMX_SMBADDR1(n)             	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x008)
#define NUCMX_SMBTMR_ST(n)				REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x009)
#define NUCMX_SMBCTL2(n)              	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x00A)
#define NUCMX_SMBTMR_EN(n)				REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x00B)
#define NUCMX_SMBADDR2(n)             	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x00C)
#define NUCMX_SMBCTL3(n)              	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x00E)
#define NUCMX_SMBADDR3(n)             	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x010)
#define NUCMX_SMBADDR7(n)             	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x011)
#define NUCMX_SMBADDR4(n)             	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x012)
#define NUCMX_SMBADDR8(n)             	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x013)
#define NUCMX_SMBADDR5(n)             	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x014)
#define NUCMX_SMBADDR6(n)             	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x016)
#define NUCMX_SMBCST2(n)              	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x018)
#define NUCMX_SMBCST3(n)              	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x019)
#define NUCMX_SMBCTL4(n)              	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x01A)
#define NUCMX_SMBSCLLT(n)             	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x01C)
#define NUCMX_SMBSCLHT(n)             	REG8 (NUCMX_SMB_BASE_ADDR(n) + 0x01E)

/* SMBus register fields */
#define NUCMX_SMBST_XMIT              	0
#define NUCMX_SMBST_MASTER            	1
#define NUCMX_SMBST_NMATCH            	2
#define NUCMX_SMBST_STASTR            	3
#define NUCMX_SMBST_NEGACK            	4
#define NUCMX_SMBST_BER               	5
#define NUCMX_SMBST_SDAST             	6
#define NUCMX_SMBST_SLVSTP            	7
#define NUCMX_SMBCST_BUSY             	0
#define NUCMX_SMBCST_BB               	1
#define NUCMX_SMBCST_MATCH            	2
#define NUCMX_SMBCST_GCMATCH          	3
#define NUCMX_SMBCST_TSDA             	4
#define NUCMX_SMBCST_TGSCL            	5
#define NUCMX_SMBCST_MATCHAF          	6
#define NUCMX_SMBCST_ARPMATCH         	7
#define NUCMX_SMBCST2_MATCHA1F        	0
#define NUCMX_SMBCST2_MATCHA2F        	1
#define NUCMX_SMBCST2_MATCHA3F        	2
#define NUCMX_SMBCST2_MATCHA4F        	3
#define NUCMX_SMBCST2_MATCHA5F        	4
#define NUCMX_SMBCST2_MATCHA6F        	5
#define NUCMX_SMBCST2_MATCHA7F        	6
#define NUCMX_SMBCST2_INTSTS          	7
#define NUCMX_SMBCST3_MATCHA8F        	0
#define NUCMX_SMBCST3_MATCHA9F        	1
#define NUCMX_SMBCST3_MATCHA10F       	2
#define NUCMX_SMBCTL1_START           	0
#define NUCMX_SMBCTL1_STOP            	1
#define NUCMX_SMBCTL1_INTEN           	2
#define NUCMX_SMBCTL1_ACK             	4
#define NUCMX_SMBCTL1_GCMEN           	5
#define NUCMX_SMBCTL1_NMINTE          	6
#define NUCMX_SMBCTL1_STASTRE         	7
#define NUCMX_SMBCTL2_ENABLE          	0
#define NUCMX_SMBCTL3_ARPMEN          	2
#define NUCMX_SMBCTL3_IDL_START       	3
#define NUCMX_SMBCTL3_400K       		4
#define NUCMX_SMBCTL3_SDA_LVL          	6
#define NUCMX_SMBCTL3_SCL_LVL          	7
#define NUCMX_SMBADDR1_SAEN           	7
#define NUCMX_SMBADDR2_SAEN           	7
#define NUCMX_SMBADDR3_SAEN           	7
#define NUCMX_SMBADDR4_SAEN           	7
#define NUCMX_SMBADDR5_SAEN           	7
#define NUCMX_SMBADDR6_SAEN           	7
#define NUCMX_SMBADDR7_SAEN           	7
#define NUCMX_SMBADDR8_SAEN           	7

/*******************************************************************************************/
/* Power Management Controller (PMC) Registers */
#define NUCMX_PMCSR                   	REG8 (NUCMX_PMC_BASE_ADDR + 0x000)
#define NUCMX_ENIDL_CTL               	REG8 (NUCMX_PMC_BASE_ADDR + 0x003)
#define NUCMX_DISIDL_CTL               	REG8 (NUCMX_PMC_BASE_ADDR + 0x004)
#define NUCMX_DISIDL_CTL1               REG8 (NUCMX_PMC_BASE_ADDR + 0x005)
#define NUCMX_PWDWN_CTL(offset)         REG8 (NUCMX_PMC_BASE_ADDR + 0x008 + offset)
#define NUCMX_PWDWN_CTL_COUNT			6

/* PMC register fields */
#define NUCMX_PMCSR_DI_INSTW          	0
#define NUCMX_PMCSR_DHF               	1
#define NUCMX_PMCSR_IDLE              	2
#define NUCMX_PMCSR_NWBI              	3
#define NUCMX_PMCSR_OHFC              	6
#define NUCMX_PMCSR_OLFC              	7
#define NUCMX_ENIDL_CTL_ADC_LFSL      	7
#define NUCMX_ENIDL_CTL_LP_WK_CTL      	6
#define NUCMX_ENIDL_CTL_PECI_ENI      	2
#define NUCMX_ENIDL_CTL_ADC_ACC_DIS   	1
#define NUCMX_PWDWN_CTL1_KBS_PD			0
#define NUCMX_PWDWN_CTL1_SDP_PD			1
#define NUCMX_PWDWN_CTL1_FIU_PD			2
#define NUCMX_PWDWN_CTL1_PS2_PD			3
#define NUCMX_PWDWN_CTL1_UART_PD		4
#define NUCMX_PWDWN_CTL1_MFT1_PD		5
#define NUCMX_PWDWN_CTL1_MFT2_PD		6
#define NUCMX_PWDWN_CTL1_MFT3_PD		7
#define NUCMX_PWDWN_CTL2_PWM0_PD		0
#define NUCMX_PWDWN_CTL2_PWM1_PD		1
#define NUCMX_PWDWN_CTL2_PWM2_PD		2
#define NUCMX_PWDWN_CTL2_PWM3_PD		3
#define NUCMX_PWDWN_CTL2_PWM4_PD		4
#define NUCMX_PWDWN_CTL2_PWM5_PD		5
#define NUCMX_PWDWN_CTL2_PWM6_PD		6
#define NUCMX_PWDWN_CTL2_PWM7_PD		7
#define NUCMX_PWDWN_CTL3_SMB0_PD		0
#define NUCMX_PWDWN_CTL3_SMB1_PD		1
#define NUCMX_PWDWN_CTL3_SMB2_PD		2
#define NUCMX_PWDWN_CTL3_SMB3_PD		3
#define NUCMX_PWDWN_CTL3_GMDA_PD		7
#define NUCMX_PWDWN_CTL4_ITIM1_PD		0
#define NUCMX_PWDWN_CTL4_ITIM2_PD		1
#define NUCMX_PWDWN_CTL4_ITIM3_PD		2
#define NUCMX_PWDWN_CTL4_ADC_PD			4
#define NUCMX_PWDWN_CTL4_PECI_PD		5
#define NUCMX_PWDWN_CTL4_PWM6_PD		6
#define NUCMX_PWDWN_CTL4_SPIP_PD		7
#define NUCMX_PWDWN_CTL5_C2HACC_PD		3
#define NUCMX_PWDWN_CTL5_SHM_REG_PD		4
#define NUCMX_PWDWN_CTL5_SHM_PD			5
#define NUCMX_PWDWN_CTL5_DP80_PD		6
#define NUCMX_PWDWN_CTL5_MSWC_PD		7
#define NUCMX_PWDWN_CTL6_ITIM4_PD		0
#define NUCMX_PWDWN_CTL6_ITIM5_PD		1
#define NUCMX_PWDWN_CTL6_ITIM6_PD		2
#define NUCMX_PWDWN_CTL6_ESPI_PD		7

/*
 * PMC enumeration
 * Offsets from CGC_BASE registers for each peripheral.
 */
enum {
	CGC_OFFSET_KBS 		=	0,
	CGC_OFFSET_UART 	=	0,
	CGC_OFFSET_FAN 		=	0,
	CGC_OFFSET_FIU 		=	0,
	CGC_OFFSET_PWM 		=	1,
	CGC_OFFSET_I2C 		=	2,
	CGC_OFFSET_ADC 		=	3,
	CGC_OFFSET_TIMER 	=	3,
	CGC_OFFSET_LPC 		=	4,
	CGC_OFFSET_ESPI 	=	5,
};

#define CGC_KBS_MASK 	(1<<NUCMX_PWDWN_CTL1_KBS_PD)
#define CGC_UART_MASK 	(1<<NUCMX_PWDWN_CTL1_UART_PD)
#define CGC_FAN_MASK 	(1<<NUCMX_PWDWN_CTL1_MFT1_PD)
#define CGC_FIU_MASK 	(1<<NUCMX_PWDWN_CTL1_FIU_PD)
#define CGC_PWM_MASK 	(1<<NUCMX_PWDWN_CTL2_PWM2_PD)
#define CGC_I2C_MASK 	((1<<NUCMX_PWDWN_CTL3_SMB0_PD) | (1<<NUCMX_PWDWN_CTL3_SMB1_PD) | (1<<NUCMX_PWDWN_CTL3_SMB2_PD))
#define CGC_ADC_MASK 	(1<<NUCMX_PWDWN_CTL4_ADC_PD)
#define CGC_TIMER_MASK 	((1<<NUCMX_PWDWN_CTL4_ITIM1_PD)  | (1<<NUCMX_PWDWN_CTL4_ITIM2_PD)   | (1<<NUCMX_PWDWN_CTL4_ITIM3_PD))
#define CGC_LPC_MASK 	((1<<NUCMX_PWDWN_CTL5_C2HACC_PD) | (1<<NUCMX_PWDWN_CTL5_SHM_REG_PD) | (1<<NUCMX_PWDWN_CTL5_SHM_PD) \
                         | (1<<NUCMX_PWDWN_CTL5_DP80_PD) | (1<<NUCMX_PWDWN_CTL5_MSWC_PD))
#define CGC_ESPI_MASK 	(1<<NUCMX_PWDWN_CTL6_ESPI_PD)

/*******************************************************************************************/
/* Flash Interface Unit (FIU) Registers */
#define NUCMX_FIU_CFG                 	REG8 (NUCMX_FIU_BASE_ADDR + 0x000)
#define NUCMX_BURST_CFG               	REG8 (NUCMX_FIU_BASE_ADDR + 0x001)
#define NUCMX_RESP_CFG                	REG8 (NUCMX_FIU_BASE_ADDR + 0x002)
#define NUCMX_SPI_FL_CFG              	REG8 (NUCMX_FIU_BASE_ADDR + 0x014)
#define NUCMX_UMA_CODE                	REG8 (NUCMX_FIU_BASE_ADDR + 0x016)
#define NUCMX_UMA_AB0                 	REG8 (NUCMX_FIU_BASE_ADDR + 0x017)
#define NUCMX_UMA_AB1                 	REG8 (NUCMX_FIU_BASE_ADDR + 0x018)
#define NUCMX_UMA_AB2                 	REG8 (NUCMX_FIU_BASE_ADDR + 0x019)
#define NUCMX_UMA_DB0                 	REG8 (NUCMX_FIU_BASE_ADDR + 0x01A)
#define NUCMX_UMA_DB1                 	REG8 (NUCMX_FIU_BASE_ADDR + 0x01B)
#define NUCMX_UMA_DB2                 	REG8 (NUCMX_FIU_BASE_ADDR + 0x01C)
#define NUCMX_UMA_DB3                 	REG8 (NUCMX_FIU_BASE_ADDR + 0x01D)
#define NUCMX_UMA_CTS                 	REG8 (NUCMX_FIU_BASE_ADDR + 0x01E)
#define NUCMX_UMA_ECTS                	REG8 (NUCMX_FIU_BASE_ADDR + 0x01F)
#define NUCMX_UMA_DB0_3               	REG32(NUCMX_FIU_BASE_ADDR + 0x020)
#define NUCMX_FIU_RD_CMD		    	REG8 (NUCMX_FIU_BASE_ADDR + 0x030)
#define NUCMX_FIU_DMM_CYC		        REG8 (NUCMX_FIU_BASE_ADDR + 0x032)
#define NUCMX_FIU_EXT_CFG		        REG8 (NUCMX_FIU_BASE_ADDR + 0x033)
#define NUCMX_FIU_UMA_AB0_3		        REG32(NUCMX_FIU_BASE_ADDR + 0x034)

/* FIU register fields */
#define NUCMX_RESP_CFG_IAD_EN         	0
#define NUCMX_RESP_CFG_DEV_SIZE_EX    	2
#define NUCMX_UMA_CTS_A_SIZE          	3
#define NUCMX_UMA_CTS_C_SIZE          	4
#define NUCMX_UMA_CTS_RD_WR           	5
#define NUCMX_UMA_CTS_DEV_NUM          	6
#define NUCMX_UMA_CTS_EXEC_DONE       	7
#define NUCMX_UMA_ECTS_SW_CS0         	0
#define NUCMX_UMA_ECTS_SW_CS1         	1
#define NUCMX_UMA_ECTS_SEC_CS         	2

/*******************************************************************************************/
/* Shared Memory (SHM) Registers */
#define NUCMX_SMC_STS               	REG8 (NUCMX_SHM_BASE_ADDR + 0x000)
#define NUCMX_SMC_CTL               	REG8 (NUCMX_SHM_BASE_ADDR + 0x001)
#define NUCMX_SHM_CTL	            	REG8 (NUCMX_SHM_BASE_ADDR + 0x002)
#define NUCMX_IMA_WIN_SIZE				REG8 (NUCMX_SHM_BASE_ADDR + 0x005)
#define NUCMX_WIN_SIZE              	REG8 (NUCMX_SHM_BASE_ADDR + 0x007)
#define NUCMX_SHAW_SEM(win)         	REG8 (NUCMX_SHM_BASE_ADDR + 0x008 + (win))
#define NUCMX_IMA_SEM		         	REG8 (NUCMX_SHM_BASE_ADDR + 0x00B)
#define NUCMX_SHCFG                 	REG8 (NUCMX_SHM_BASE_ADDR + 0x00E)
#define NUCMX_WIN_WR_PROT(win)         	REG8 (NUCMX_SHM_BASE_ADDR + 0x010 + (win*2L))
#define NUCMX_WIN_RD_PROT(win)         	REG8 (NUCMX_SHM_BASE_ADDR + 0x011 + (win*2L))
#define NUCMX_IMA_WR_PROT	         	REG8 (NUCMX_SHM_BASE_ADDR + 0x016)
#define NUCMX_IMA_RD_PROT	         	REG8 (NUCMX_SHM_BASE_ADDR + 0x017)
#define NUCMX_WIN_BASE(win)         	REG32 (NUCMX_SHM_BASE_ADDR + 0x020 + (win*4L))

#define NUCMX_PWIN_BASEI(win)           REG16(NUCMX_SHM_BASE_ADDR + 0x020 + (win*4L))
#define NUCMX_PWIN_SIZEI(win)           REG16(NUCMX_SHM_BASE_ADDR + 0x022 + (win*4L))

#define NUCMX_IMA_BASE		         	REG32(NUCMX_SHM_BASE_ADDR + 0x02C)
#define NUCMX_RST_CFG		         	REG8 (NUCMX_SHM_BASE_ADDR + 0x03A)
#define NUCMX_DP80BUF		         	REG16(NUCMX_SHM_BASE_ADDR + 0x040)
#define NUCMX_DP80STS               	REG8 (NUCMX_SHM_BASE_ADDR + 0x042)
#define NUCMX_DP80CTL               	REG8 (NUCMX_SHM_BASE_ADDR + 0x044)
#define NUCMX_HOFS_STS					REG8 (NUCMX_SHM_BASE_ADDR + 0x048)
#define NUCMX_HOFS_CTL					REG8 (NUCMX_SHM_BASE_ADDR + 0x049)
#define NUCMX_COFS2						REG16(NUCMX_SHM_BASE_ADDR + 0x04A)
#define NUCMX_COFS1						REG16(NUCMX_SHM_BASE_ADDR + 0x04C)
#define NUCMX_IHOFS2					REG16(NUCMX_SHM_BASE_ADDR + 0x050)
#define NUCMX_IHOFS1					REG16(NUCMX_SHM_BASE_ADDR + 0x052)
#define NUCMX_SHM_VER					REG8 (NUCMX_SHM_BASE_ADDR + 0x07F)


/* SHM register fields */
#define NUCMX_SMC_STS_HRERR           	0
#define NUCMX_SMC_STS_HWERR           	1
#define NUCMX_SMC_STS_HSEM1W          	4
#define NUCMX_SMC_STS_HSEM2W          	5
#define NUCMX_SMC_STS_SHM_ACC         	6
#define NUCMX_SMC_CTL_HERR_IE         	2
#define NUCMX_SMC_CTL_HSEM1_IE        	3
#define NUCMX_SMC_CTL_HSEM2_IE        	4
#define NUCMX_SMC_CTL_ACC_IE          	5
#define NUCMX_SMC_CTL_PREF_EN         	6
#define NUCMX_SMC_CTL_HOSTWAIT        	7
#define NUCMX_FLASH_SIZE_STALL_HOST   	6
#define NUCMX_FLASH_SIZE_RD_BURST     	7
#define NUCMX_WIN_PROT_RW1L_RP        	0
#define NUCMX_WIN_PROT_RW1L_WP        	1
#define NUCMX_WIN_PROT_RW1H_RP        	2
#define NUCMX_WIN_PROT_RW1H_WP        	3
#define NUCMX_WIN_PROT_RW2L_RP        	4
#define NUCMX_WIN_PROT_RW2L_WP        	5
#define NUCMX_WIN_PROT_RW2H_RP        	6
#define NUCMX_WIN_PROT_RW2H_WP        	7
#define NUCMX_PWIN_SIZEI_RPROT			13
#define NUCMX_PWIN_SIZEI_WPROT			14
#define	NUCMX_CSEM2					  	6
#define	NUCMX_CSEM3					  	7
#define NUCMX_DP80STS_FWR             	5
#define NUCMX_DP80STS_FNE             	6
#define NUCMX_DP80STS_FOR             	7
#define NUCMX_DP80CTL_DP80EN          	0
#define NUCMX_DP80CTL_SYNCEN          	1
#define NUCMX_DP80CTL_RFIFO           	4
#define NUCMX_DP80CTL_CIEN            	5

/*******************************************************************************************/
/* KBC Registers */
#define NUCMX_HICTRL                   	REG8(NUCMX_KBC_BASE_ADDR + 0x000)
#define NUCMX_HIIRQC                   	REG8(NUCMX_KBC_BASE_ADDR + 0x002)
#define NUCMX_HIKMST                   	REG8(NUCMX_KBC_BASE_ADDR + 0x004)
#define NUCMX_HIKDO                   	REG8(NUCMX_KBC_BASE_ADDR + 0x006)
#define NUCMX_HIMDO                   	REG8(NUCMX_KBC_BASE_ADDR + 0x008)
#define NUCMX_KBCVER                   	REG8(NUCMX_KBC_BASE_ADDR + 0x009)
#define NUCMX_HIKMDI                   	REG8(NUCMX_KBC_BASE_ADDR + 0x00A)
#define NUCMX_SHIKMDI                  	REG8(NUCMX_KBC_BASE_ADDR + 0x00B)

/* KBC register field */
#define NUCMX_HICTRL_OBFKIE				0
#define NUCMX_HICTRL_OBFMIE				1

/*******************************************************************************************/
/* PM Channel Registers */
#define NUCMX_HIPMST(n)                 REG8(NUCMX_PM_CH_BASE_ADDR(n) + 0x000)
#define NUCMX_HIPMDO(n)                 REG8(NUCMX_PM_CH_BASE_ADDR(n) + 0x002)
#define NUCMX_HIPMDI(n)                 REG8(NUCMX_PM_CH_BASE_ADDR(n) + 0x004)
#define NUCMX_SHIPMDI(n)                REG8(NUCMX_PM_CH_BASE_ADDR(n) + 0x005)
#define NUCMX_HIPMDOC(n)             	REG8(NUCMX_PM_CH_BASE_ADDR(n) + 0x006)
#define NUCMX_HIPMDOM(n)             	REG8(NUCMX_PM_CH_BASE_ADDR(n) + 0x008)
#define NUCMX_HIPMDIC(n)                REG8(NUCMX_PM_CH_BASE_ADDR(n) + 0x00A)
#define NUCMX_HIPMCTL(n)                REG8(NUCMX_PM_CH_BASE_ADDR(n) + 0x00C)
#define NUCMX_HIPMCTL2(n)               REG8(NUCMX_PM_CH_BASE_ADDR(n) + 0x00D)
#define NUCMX_HIPMIC(n)                 REG8(NUCMX_PM_CH_BASE_ADDR(n) + 0x00E)
#define NUCMX_HIPMIE(n)                 REG8(NUCMX_PM_CH_BASE_ADDR(n) + 0x010)

/* PM Channel register field */
#define NUCMX_HIPMIE_SCIE				1
#define NUCMX_HIPMIE_SMIE				2

/*
 * PM Channel enumeration
 */
enum PM_CHANNEL_T {
	PM_CHAN_1,
	PM_CHAN_2,
	PM_CHAN_3,
	PM_CHAN_4
};

/*******************************************************************************************/
/* SuperI/O Internal Bus (SIB) Registers */
#define NUCMX_IHIOA                   	REG16(NUCMX_SIB_BASE_ADDR + 0x000)
#define NUCMX_IHD                     	REG8 (NUCMX_SIB_BASE_ADDR + 0x002)
#define NUCMX_LKSIOHA                 	REG16(NUCMX_SIB_BASE_ADDR + 0x004)
#define NUCMX_SIOLV                   	REG16(NUCMX_SIB_BASE_ADDR + 0x006)
#define NUCMX_CRSMAE                  	REG16(NUCMX_SIB_BASE_ADDR + 0x008)
#define NUCMX_SIBCTRL                 	REG8 (NUCMX_SIB_BASE_ADDR + 0x00A)
#define NUCMX_C2H_VER                 	REG8 (NUCMX_SIB_BASE_ADDR + 0x00E)
/* SIB register fields  */
#define NUCMX_SIBCTRL_CSAE            	0
#define NUCMX_SIBCTRL_CSRD            	1
#define NUCMX_SIBCTRL_CSWR            	2
#define NUCMX_LKSIOHA_LKCFG				0
#define NUCMX_CRSMAE_CFGAE				0

/*******************************************************************************************/
/* Battery-Backed RAM (BBRAM) Registers */
#define NUCMX_BKUP_STS                  REG8 (NUCMX_BBRAM_BASE_ADDR + 0x000)
#define NUCMX_BBRAM(offset)				REG8 (NUCMX_BBRAM_BASE_ADDR + 0x001 + offset)

/* BBRAM register fields */
#define NUCMX_BKUP_STS_IBBR           	7
#define NUCMX_BBRAM_SIZE 				63  /* Size of BBRAM */

/*******************************************************************************************/
/* Timer Watch Dog (TWD) Registers */
#define NUCMX_TWCFG                   	REG8 (NUCMX_TWD_BASE_ADDR + 0x000)
#define NUCMX_TWCP                    	REG8 (NUCMX_TWD_BASE_ADDR + 0x002)
#define NUCMX_TWDT0                   	REG16(NUCMX_TWD_BASE_ADDR + 0x004)
#define NUCMX_T0CSR                   	REG8 (NUCMX_TWD_BASE_ADDR + 0x006)
#define NUCMX_WDCNT                   	REG8 (NUCMX_TWD_BASE_ADDR + 0x008)
#define NUCMX_WDSDM                   	REG8 (NUCMX_TWD_BASE_ADDR + 0x00A)
#define NUCMX_TWMT0                   	REG16(NUCMX_TWD_BASE_ADDR + 0x00C)
#define NUCMX_TWMWD                   	REG8 (NUCMX_TWD_BASE_ADDR + 0x00E)
#define NUCMX_WDCP                    	REG8 (NUCMX_TWD_BASE_ADDR + 0x010)

/* TWD register fields */
#define NUCMX_TWCFG_LTWCFG            	0
#define NUCMX_TWCFG_LTWCP             	1
#define NUCMX_TWCFG_LTWDT0            	2
#define NUCMX_TWCFG_LWDCNT            	3
#define NUCMX_TWCFG_WDCT0I            	4
#define NUCMX_TWCFG_WDSDME            	5
#define NUCMX_TWCFG_WDRST_MODE        	6
#define	NUCMX_TWCFG_WDC2POR				7
#define NUCMX_T0CSR_RST               	0
#define NUCMX_T0CSR_TC                	1
#define NUCMX_T0CSR_WDLTD             	3
#define NUCMX_T0CSR_WDRST_STS         	4
#define NUCMX_T0CSR_WD_RUN            	5
#define NUCMX_T0CSR_TESDIS            	7

/*******************************************************************************************/
/* ADC Registers */
#define NUCMX_ADCSTS                    REG16(NUCMX_ADC_BASE_ADDR + 0x000)
#define NUCMX_ADCCNF                 	REG16(NUCMX_ADC_BASE_ADDR + 0x002)
#define NUCMX_ATCTL                   	REG16(NUCMX_ADC_BASE_ADDR + 0x004)
#define NUCMX_ASCADD                 	REG16(NUCMX_ADC_BASE_ADDR + 0x006)
#define NUCMX_ADCCS                  	REG16(NUCMX_ADC_BASE_ADDR + 0x008)
#define NUCMX_CHNDAT(n)                 REG16(NUCMX_ADC_BASE_ADDR + 0x040 + (2L*(n)))
#define NUCMX_ADCCNF2                  	REG16(NUCMX_ADC_BASE_ADDR + 0x020)
#define NUCMX_GENDLY                  	REG16(NUCMX_ADC_BASE_ADDR + 0x022)
#define NUCMX_MEAST                  	REG16(NUCMX_ADC_BASE_ADDR + 0x026)

/* ADC register fields */
#define NUCMX_ATCTL_SCLKDIV             0
#define NUCMX_ATCTL_DLY                 8
#define NUCMX_ASCADD_SADDR              0
#define NUCMX_ADCSTS_EOCEV              0
#define NUCMX_ADCCNF_ADCMD              1
#define NUCMX_ADCCNF_ADCRPTC            3
#define NUCMX_ADCCNF_INTECEN            6
#define NUCMX_ADCCNF_START              4
#define NUCMX_ADCCNF_ADCEN              0
#define NUCMX_ADCCNF_STOP               11
#define NUCMX_CHNDAT_CHDAT              0
#define NUCMX_CHNDAT_NEW                15
/*******************************************************************************************/
/* SPI Register */
#define NUCMX_SPI_DATA       			REG16(NUCMX_SPI_BASE_ADDR + 0x00)
#define NUCMX_SPI_CTL1       			REG16(NUCMX_SPI_BASE_ADDR + 0x02)
#define NUCMX_SPI_STAT       			REG8(NUCMX_SPI_BASE_ADDR + 0x04)

/* SPI register fields */
#define NUCMX_SPI_CTL1_SPIEN            0
#define NUCMX_SPI_CTL1_SNM              1
#define NUCMX_SPI_CTL1_MOD              2 
#define NUCMX_SPI_CTL1_EIR              5 
#define NUCMX_SPI_CTL1_EIW              6
#define NUCMX_SPI_CTL1_SCM              7
#define NUCMX_SPI_CTL1_SCIDL            8
#define NUCMX_SPI_CTL1_SCDV             9
#define NUCMX_SPI_STAT_BSY              0
#define NUCMX_SPI_STAT_RBF              1

/*******************************************************************************************/
/* PECI Registers */

#define NUCMX_PECI_CTL_STS             	REG8(NUCMX_PECI_BASE_ADDR + 0x000)
#define NUCMX_PECI_RD_LENGTH           	REG8(NUCMX_PECI_BASE_ADDR + 0x001)
#define NUCMX_PECI_ADDR                	REG8(NUCMX_PECI_BASE_ADDR + 0x002)
#define NUCMX_PECI_CMD                 	REG8(NUCMX_PECI_BASE_ADDR + 0x003)
#define NUCMX_PECI_CTL2                	REG8(NUCMX_PECI_BASE_ADDR + 0x004)
#define NUCMX_PECI_INDEX               	REG8(NUCMX_PECI_BASE_ADDR + 0x005)
#define NUCMX_PECI_IDATA               	REG8(NUCMX_PECI_BASE_ADDR + 0x006)
#define NUCMX_PECI_WR_LENGTH           	REG8(NUCMX_PECI_BASE_ADDR + 0x007)
#define NUCMX_PECI_CFG                	REG8(NUCMX_PECI_BASE_ADDR + 0x009)
#define NUCMX_PECI_RATE                	REG8(NUCMX_PECI_BASE_ADDR + 0x00F)
#define NUCMX_PECI_DATA_IN(i)          	REG8(NUCMX_PECI_BASE_ADDR + 0x010 + (i))
#define NUCMX_PECI_DATA_OUT(i)         	REG8(NUCMX_PECI_BASE_ADDR + 0x010 + (i))

/* PECI register fields */
#define NUCMX_PECI_CTL_STS_START_BUSY  	0
#define NUCMX_PECI_CTL_STS_DONE         1
#define NUCMX_PECI_CTL_STS_AVL_ERR      2
#define NUCMX_PECI_CTL_STS_CRC_ERR      3
#define NUCMX_PECI_CTL_STS_ABRT_ERR     4
#define NUCMX_PECI_CTL_STS_AWFCS_EN     5
#define NUCMX_PECI_CTL_STS_DONE_EN      6
#define NUCMX_ESTRPST_PECIST          	0
#define SFT_STRP_CFG_CK50       		5

/*******************************************************************************************/
/* PWM Registers */
#define NUCMX_PRSC(n)                   REG16(NUCMX_PWM_BASE_ADDR(n) + 0x000)
#define NUCMX_CTR(n)                 	REG16(NUCMX_PWM_BASE_ADDR(n) + 0x002)
#define NUCMX_PWMCTL(n)                 REG8(NUCMX_PWM_BASE_ADDR(n) + 0x004)
#define NUCMX_DCR(n)                 	REG16(NUCMX_PWM_BASE_ADDR(n) + 0x006)
#define NUCMX_PWMCTLEX(n)               REG8(NUCMX_PWM_BASE_ADDR(n) + 0x00C)

/* PWM register fields */
#define NUCMX_PWMCTL_INVP               0
#define NUCMX_PWMCTL_CKSEL              1
#define NUCMX_PWMCTL_HB_DC_CTL          2
#define NUCMX_PWMCTL_PWR                7
#define NUCMX_PWMCTLEX_FCK_SEL          4
#define NUCMX_PWMCTLEX_OD_OUT           7
/*******************************************************************************************/
/* MFT Registers */
#define NUCMX_TCNT1(n)                	REG16 (NUCMX_MFT_BASE_ADDR(n) + 0x000)
#define NUCMX_TCRA(n)                   REG16(NUCMX_MFT_BASE_ADDR(n) + 0x002)
#define NUCMX_TCRB(n)                   REG16(NUCMX_MFT_BASE_ADDR(n) + 0x004)
#define NUCMX_TCNT2(n)                	REG16 (NUCMX_MFT_BASE_ADDR(n) + 0x006)
#define NUCMX_TPRSC(n)                	REG8  (NUCMX_MFT_BASE_ADDR(n) + 0x008)
#define NUCMX_TCKC(n)                	REG8  (NUCMX_MFT_BASE_ADDR(n) + 0x00A)
#define	NUCMX_TMCTRL(n)					REG8  (NUCMX_MFT_BASE_ADDR(n) + 0x00C)
#define NUCMX_TECTRL(n)                	REG8  (NUCMX_MFT_BASE_ADDR(n) + 0x00E)
#define NUCMX_TECLR(n)                	REG8  (NUCMX_MFT_BASE_ADDR(n) + 0x010)
#define	NUCMX_TIEN(n)					REG8  (NUCMX_MFT_BASE_ADDR(n) + 0x012)
#define	NUCMX_TWUEN(n)					REG8  (NUCMX_MFT_BASE_ADDR(n) + 0x01A)
#define	NUCMX_TCFG(n)					REG8  (NUCMX_MFT_BASE_ADDR(n) + 0x01C)

/* MFT register fields */
#define NUCMX_TMCTRL_MDSEL              0
#define NUCMX_TCKC_LOW_PWR              7
#define NUCMX_TCKC_PLS_ACC_CLK          6
#define NUCMX_TCKC_C1CSEL               0
#define NUCMX_TCKC_C2CSEL               3
#define NUCMX_TMCTRL_TAEN               5
#define NUCMX_TMCTRL_TBEN               6
#define NUCMX_TMCTRL_TAEDG              3
#define NUCMX_TMCTRL_TBEDG              4
#define NUCMX_TCFG_TADBEN               6
#define NUCMX_TCFG_TBDBEN               7
#define NUCMX_TECTRL_TAPND              0
#define NUCMX_TECTRL_TBPND              1
#define NUCMX_TECTRL_TCPND              2
#define NUCMX_TECTRL_TDPND              3
#define NUCMX_TECLR_TACLR               0
#define NUCMX_TECLR_TBCLR               1
#define NUCMX_TECLR_TCCLR               2
#define NUCMX_TECLR_TDCLR               3
#define NUCMX_TIEN_TAIEN                0
#define NUCMX_TIEN_TBIEN                1
#define NUCMX_TIEN_TCIEN                2
#define NUCMX_TIEN_TDIEN                3
#define	NUCMX_TWUEN_TAWEN               0
#define	NUCMX_TWUEN_TBWEN               1
#define	NUCMX_TWUEN_TCWEN               2
#define	NUCMX_TWUEN_TDWEN               3
/*******************************************************************************************/
/*ITIM16 Define*/
#define ITIM16_INT(module) 				CONCAT2(NUCMX_IRQ_, module)

/* ITIM16 registers */
#define NUCMX_ITCNT(n)					REG8  (NUCMX_ITIM16_BASE_ADDR(n) + 0x000)
#define NUCMX_ITPRE(n)					REG8  (NUCMX_ITIM16_BASE_ADDR(n) + 0x001)
#define NUCMX_ITCNT16(n)				REG16 (NUCMX_ITIM16_BASE_ADDR(n) + 0x002)
#define NUCMX_ITCTS(n)					REG8  (NUCMX_ITIM16_BASE_ADDR(n) + 0x004)

/* ITIM16 register fields */
#define	NUCMX_ITIM16_TO_STS				0
#define	NUCMX_ITIM16_TO_IE				2
#define	NUCMX_ITIM16_TO_WUE				3
#define	NUCMX_ITIM16_CKSEL				4
#define	NUCMX_ITIM16_ITEN				7

/* ITIM16 enumeration*/
enum ITIM16_MODULE_T
{
    ITIM16_1,
    ITIM16_2,
    ITIM16_3,
    ITIM16_4,
    ITIM16_5,
    ITIM16_6,
    ITIM16_MODULE_COUNT,
};

/*******************************************************************************************/
/* Monotonic Counter (MTC) Registers */
#define NUCMX_TTC                  		REG32(NUCMX_MTC_BASE_ADDR + 0x000)
#define NUCMX_WTC                  		REG32(NUCMX_MTC_BASE_ADDR + 0x004)
#define NUCMX_MTCTST               		REG8 (NUCMX_MTC_BASE_ADDR + 0x008)
#define NUCMX_MTCVER               		REG8 (NUCMX_MTC_BASE_ADDR + 0x00C)

/* MTC register fields */
#define NUCMX_WTC_PTO           		30
#define NUCMX_WTC_WIE           		31

#endif /* __CROS_EC_REGISTERS_H */
