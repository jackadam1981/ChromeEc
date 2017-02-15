/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ESPI module for Chrome EC */

#include "common.h"
#include "acpi.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "keyboard_protocol.h"
#include "port80.h"
#include "util.h"
#include "chipset.h"

#include "registers.h"
#include "espi.h"
#include "lpc.h"
#include "lpc_chip.h"
#include "system.h"
#include "task.h"
#include "console.h"
#include "uart.h"
#include "util.h"
#include "power.h"
#include "timer.h"


/* Console output macros */
#if !(DEBUG_ESPI)
#define CPUTS(...)
#define CPRINTS(...)
#else
#if 0
#define CPUTS(outstr) cputs(CC_LPC, outstr)
#define CPRINTS(format, args...) cprints(CC_LPC, format, ## args)
#else
#define CPUTS(...)
#define CPRINTS(...)
#endif
#endif

/* #ifdef CONFIG_ESPI */

/* TODO */
/* MCHP TODO */
#define CONFIG_ESPI_CAP
#define CONFIG_ESPI_CAP0_VAL 0x0F
#define CONFIG_ESPI_CAP1_VAL 0x00

static uint32_t espi_channels_ready;

/*
 * eSPI Virtual Wire reset values
 * VWire name used by chip independent code.
 * Host eSPI Master VWire index containing signal
 * Reset value of VWire. Note, each Host VWire index may
 * have a different reset source:
 *	EC Power-on/chip reset
 *	ESPI_RESET# assertion by Host eSPI master
 *	eSPI Platform Reset assertion by Host eSPI master
 *		MEC1701H allows eSPI Platform reset to
 *		be a VWire or side band signal.
 *
 * NOTE MEC1701H Boot-ROM will restore VWires ... from
 * VBAT power register MEC17XX_VBAT_VWIRE_BACKUP.
 *	bits[3:0] = Master-to-Slave Index 02h SRC3:SRC0 values
 *		MSVW00 register
 *			SRC0 = SLP_S3#
 *			SRC1 = SLP_S4#
 *			SRC2 = SLP_S5#
 *			SRC3 = reserved
 *	bits[7:4] = Master-to-Slave Index 42h SRC3:SRC0 values
 *		MSVW04 register
 *			SRC0 = SLP_LAN#
 *			SRC1 = SLP_WLAN#
 *			SRC2 = reserved
 *			SRC3 = reserved
 *
 */
struct vw_info_t {
	uint16_t name;		/* signal name */
	uint8_t  host_idx;	/* Host VWire index of signal */
	uint8_t  reset_val;	/* reset value of VWire */
	uint8_t  flags;		/* b[0]=0(MSVW), =1(SMVW) */
	uint8_t  reg_idx;	/* MSVW or SMVW index */
	uint8_t  src_num;	/* SRC number */
	uint8_t  rsvd;
	uint8_t  *name_str;
};


/* VW signals used in eSPI */
/*
 * MEC1701H VWire mapping based on eSPI Spec 1.0,
 * eSPI Compatibility spec 0.96,
 * MEC17xx HW defaults and ec/include/espi.h
 *
 * MSVW00 index=02h PORValue=00000000_04040404_00000102 reset=RESET_SYS
 * 	SRC0 = VW_SLP_S3_L, IntrDis
 * 	SRC1 = VW_SLP_S4_L, IntrDis
 * 	SRC2 = VW_SLP_S5_L, IntrDis
 * 	SRC3 = reserved, IntrDis
 * MSVW01 index=03h PORValue=00000000_04040404_00000003 reset=RESET_ESPI
 * 	SRC0 = VW_SUS_STAT_L, IntrDis
 * 	SRC1 = VW_PLTRST_L, IntrDis
 * 	SRC2 = VW_OOB_RST_WARN, IntrDis
 * 	SRC3 = reserved, IntrDis
 * MSVW02 index=07h PORValue=00000000_04040404_00000307 reset=PLTRST
 * 	SRC0 = VW_HOST_RST_WARN
 * 	SRC1 = 0 reserved
 * 	SRC2 = 0 reserved
 * 	SRC3 = 0 reserved
 * MSVW03 index=41h PORValue=00000000_04040404_00000041 reset=RESET_ESPI
 * 	SRC0 = VW_SUS_WARN_L, IntrDis
 * 	SRC1 = VW_SUS_PWRDN_ACK_L, IntrDis
 * 	SRC2 = 0 reserved, IntrDis
 * 	SRC3 = VW_SLP_A_L, IntrDis
 * MSVW04 index=42h PORValue=00000000_04040404_00000141 reset=RESET_SYS
 * 	SRC0 = VW_SLP_LAN, IntrDis
 * 	SRC1 = VW_SLP_WLAN, IntrDis
 * 	SRC2 = reserved, IntrDis
 * 	SRC3 = reserved, IntrDis
 *
 * SMVW00 index=04h PORValue=01010000_0000C004 STOM=1100 reset=RESET_ESPI
 * 	SRC0 = VW_OOB_RST_ACK
 * 	SRC1 = 0 reserved
 * 	SRC2 = VW_WAKE_L
 * 	SRC3 = VW_PME_L
 * SMVW01 index=05h PORValue=00000000_00000005 STOM=0000 reset=RESET_ESPI
 * 	SRC0 = SLAVE_BOOT_LOAD_DONE   !!! NOTE: Google combines SRC0 & SRC3
 * 	SRC1 = VW_ERROR_FATAL
 * 	SRC2 = VW_ERROR_NON_FATAL
 * 	SRC3 = SLAVE_BOOT_LOAD_STATUS !!! into VW_SLAVE_BTLD_STATUS_DONE
 * SMVW02 index=06h PORValue=00010101_00007306 STOM=0111 reset=PLTRST
 * 	SRC0 = VW_SCI_L
 * 	SRC1 = VW_SMI_L
 * 	SRC2 = VW_RCIN_L
 * 	SRC3 = VW_HOST_RST_ACK
 * SMVW03 index=40h PORValue=00000000_00000040 STOM=0000 reset=RESET_ESPI
 * 	SRC0 = assign VW_SUS_ACK
 * 	SRC1 = 0
 * 	SRC2 = 0
 * 	SRC3 = 0
 *
 * table of vwire structures
 * MSVW00 at 0x400F9C00 offset = 0x000
 * MSVW01 at 0x400F9C0C offset = 0x00C
 * ...
 * SMVW00 at 0x400F9E00 offset = 0x200
 * SMVW01 at 0x400F9E08 offset = 0x208
 * ...
 *
 */

/* TODO finish table values */
static const struct vw_info_t vw_info_tbl[] = {
	/* name,                    host  reset       reg   SRC
	 *                          index value flags index num   rsvd */
	/* MSVW00 */
	{VW_SLP_S3_L,               0x02, 0x00, 0x00, 0x00, 0x00, 0x00, "VW_SLP_S3_L"}, /* index 02h (In)  */
	{VW_SLP_S4_L,               0x02, 0x00, 0x00, 0x00, 0x01, 0x00, "VW_SLP_S4_L"},
	{VW_SLP_S5_L,               0x02, 0x00, 0x10, 0x00, 0x02, 0x00, "VW_SLP_S5_L"},
	/* MSVW01 */
	{VW_SUS_STAT_L,             0x03, 0x00, 0x10, 0x01, 0x00, 0x00, "VW_SUS_STAT_L"}, /* index 03h (In)  */
	{VW_PLTRST_L,               0x03, 0x00, 0x10, 0x01, 0x01, 0x00, "VW_PLTRST_L"},
	{VW_OOB_RST_WARN,           0x03, 0x00, 0x10, 0x01, 0x02, 0x00, "VW_OOB_RST_WARN"},
	/* SMVW00 */
	{VW_OOB_RST_ACK,            0x04, 0x00, 0x01, 0x00, 0x00, 0x00, "VW_OOB_RST_ACK"}, /* index 04h (Out) */
	{VW_WAKE_L,                 0x04, 0x01, 0x01, 0x00, 0x02, 0x00, "VW_WAKE_L"},
	{VW_PME_L,                  0x04, 0x01, 0x01, 0x00, 0x03, 0x00, "VW_PME_L"},
	/* SMVW01 */
	{VW_ERROR_FATAL,            0x05, 0x00, 0x01, 0x01, 0x01, 0x00, "VW_ERROR_FATAL"}, /* index 05h (Out) */
	{VW_ERROR_NON_FATAL,        0x05, 0x00, 0x01, 0x01, 0x02, 0x00, "VW_ERROR_NON_FATAL"},
	{VW_SLAVE_BTLD_STATUS_DONE, 0x05, 0x00, 0x01, 0x01, 0x30, 0x00, "VW_SLAVE_BTLD_STATUS_DONE"}, /* !!! b[0]=SLAVE_BOOT_DONE, b[3]=SLAVE_BOOT_LOAD_STATUS */
	/* SMVW02 */
	{VW_SCI_L,                  0x06, 0x01, 0x01, 0x02, 0x00, 0x00, "VW_SCI_L"}, /* index 06h (Out) */
	{VW_SMI_L,                  0x06, 0x01, 0x01, 0x02, 0x01, 0x00, "VW_SMI_L"},
	{VW_RCIN_L,                 0x06, 0x01, 0x01, 0x02, 0x02, 0x00, "VW_RCIN_L"},
	{VW_HOST_RST_ACK,           0x06, 0x00, 0x01, 0x02, 0x03, 0x00, "VW_HOST_RST_ACK"},
	/* MSVW02 */
	{VW_HOST_RST_WARN,          0x07, 0x00, 0x10, 0x02, 0x00, 0x00, "VW_HOST_RST_WARN"}, /* index 07h (In)  */
	/* SMVW03 */
	{VW_SUS_ACK,                0x40, 0x00, 0x01, 0x03, 0x00, 0x00, "VW_SUS_ACK"}, /* index 40h (Out) */
	/* MSVW03 */
	{VW_SUS_WARN_L,             0x41, 0x00, 0x10, 0x03, 0x00, 0x00, "VW_SUS_WARN_L"}, /* index 41h (In)  */
	{VW_SUS_PWRDN_ACK_L,        0x41, 0x00, 0x10, 0x03, 0x01, 0x00, "VW_SUS_PWRDN_ACK_L"},
	{VW_SLP_A_L,                0x41, 0x00, 0x10, 0x03, 0x03, 0x00, "VW_SLP_A_L"},
	/* MSVW04 */
	{VW_SLP_LAN,                0x42, 0x00, 0x10, 0x04, 0x00, 0x00, "VW_SLP_LAN"}, /* index 42h (In)  */
	{VW_SLP_WLAN,               0x42, 0x00, 0x10, 0x04, 0x01, 0x00, "VW_SLP_WLAN"}
};

/* Flag for SLAVE_BOOT_LOAD siganls */
// static uint8_t boot_load_done;

/*****************************************************************************/
/* eSPI internal utilities */

static int espi_vw_get_signal_index(enum espi_vw_signal event)
{
	int i;

	/* Search table by signal name */
	for (i = 0; i < ARRAY_SIZE(vw_info_tbl); i++) {
		if (vw_info_tbl[i].name == event) {
			return i;
		}
	}

	return -1;
}


/*
 * Initialize eSPI hardware upon ESPI_RESET# de-assertion
 */
#if 0
static void espi_reset_deassert_init(void)
{

}
#endif

/* TODO - Call this on entry to deepest sleep state with EC
 * turned off.
 *
 * Save Master-to-Slave VWire Index 02h & 42h before
 * entering a deep sleep state where EC power is shut off.
 * PCH requires we restore these VWires on wake.
 * SLP_S3#, SLP_S4#, SLP_S5# in index 02h
 * SLP_LAN#, SLP_WLAN# in index 42h
 * Current VWire states are saved to a batter backed 8-bit
 * register in MEC1701H.
 * If a VBAT POR occurs the value of this register = 0 which
 * is the default state of the above VWires on a hardware
 * POR.
 */
#if 0
static void espi_vw_save(void)
{
	volatile struct mec17xx_espi_msvw *msvw;
	uint32_t i, r;
	uint8_t vb;

	msvw = (volatile struct mec17xx_espi_msvw *)(MEC17XX_ESPI_MSVW_BASE);

	vb = 0;
	r = msvw[MSVW_I42].w2;
	for (i = 0; i < 4; i++) {
		if (r & (1ul << (i << 3))) {
			vb |= (1u << i);
		}
	}

	vb <<= 4;
	r = msvw[MSVW_I02].w2;
	for (i = 0; i < 4; i++) {
		if (r & (1ul << (i << 3))) {
			vb |= (1u << i);
		}
	}

	MEC17XX_VBAT_VWIRE_BACKUP = vb;
}
#endif

/*
 * Update MEC1701H VBAT powered VWire backup values restored on MEC17xx chip
 * reset. MEC17xx Boot-ROM loads these values into MSVW00 SRC[0:3](Index 02h)
 * and MSVW04 SRC[0:3](Index 42h) on chip reset(POR, WDT reset, chip reset,
 * wake from EC off).
 * Always clear backup value after restore.
 */
static void espi_vw_restore(void)
{
	volatile struct mec17xx_espi_msvw *msvw;
	uint32_t i, r;
	uint8_t vb;

	msvw = (volatile struct mec17xx_espi_msvw *)(MEC17XX_ESPI_MSVW_BASE);

	vb = MEC17XX_VBAT_VWIRE_BACKUP;
	r = 0;
	for (i = 0; i < 4; i++) {
		if (vb & (1u << i)) {
			r |= (1ul << (i << 3));
		}
	}
	msvw[MSVW_I02].w2 = r;
	CPRINTS("eSPI restore MSVW00(Index 02h) = 0x%08x",r);
	TRACE11(28, ESPI, 0, "eSPI restore MSVW00(Index 02h) = 0x%08x",r);

	vb >>= 4;
	r = 0;
	for (i = 0; i < 4; i++) {
		if (vb & (1u << i)) {
			r |= (1ul << (i << 3));
		}
	}
	msvw[MSVW_I42].w2 = r;
	CPRINTS("eSPI restore MSVW00(Index 42h) = 0x%08x",r);
	TRACE11(29, ESPI, 0, "eSPI restore MSVW04(Index 42h) = 0x%08x",r);

	MEC17XX_VBAT_VWIRE_BACKUP = 0;
}


/*
 * Called before releasing RSMRST#
 *	ESPI_RESET# is asserted
 *	PLATFORM_RESET# is asserted
 */
static void espi_bar_pre_init(void)
{
	/* Configuration IO BAR set to 0x2E/0x2F */
	MEC17XX_ESPI_IO_BAR_ADDR_LSB(MEC17XX_ESPI_IO_BAR_ID_CFG_PORT) = 0x2E;
	MEC17XX_ESPI_IO_BAR_ADDR_MSB(MEC17XX_ESPI_IO_BAR_ID_CFG_PORT) = 0x00;
	MEC17XX_ESPI_IO_BAR_VALID(MEC17XX_ESPI_IO_BAR_ID_CFG_PORT) = 1;
}

/*
 * Called before releasing RSMRST#
 *	ESPI_RESET# is asserted
 *	PLATFORM_RESET# is asserted
 * Set all MSVW to either edge interrupt
 *	IRQ_SELECT fields are reset on RESET_SYS not ESPI_RESET or PLTRST
 *
 */
static void espi_vw_pre_init(void)
{
	volatile struct mec17xx_espi_msvw *msvw;
	uint32_t i;

	CPRINTS("eSPI VW Pre-Init");
	TRACE0(30, ESPI, 0, "eSPI VW Pre-Init");

	espi_vw_restore();

	msvw = (volatile struct mec17xx_espi_msvw *)(MEC17XX_ESPI_MSVW_BASE);

	for (i = 0; i < MSVW_MAX; i++) {
		msvw[i].w1 = 0x0f0f0f0ful; /* disable */
	}

	/* clear spurious status */
	MEC17XX_INT_SOURCE(24) = 0xfffffffful;
	MEC17XX_INT_SOURCE(25) = 0xfffffffful;

#if 0
	for (i = 0; i < ARRAY_SIZE(vw_info_tbl); i++) {
		if (vw_info_tbl[i].flags & 0x01) {
			espi_vw_enable_wire_int(vw_info_tbl[i].name);
		}
	}
#else
	msvw[MSVW_I02].w1 = 0x040f0f0ful; /* MSVW00 */
	msvw[MSVW_I03].w1 = 0x040f0f0ful; /* MSVW01 */
	msvw[MSVW_I07].w1 = 0x0404040ful; /* MSVW02 */
	msvw[MSVW_I41].w1 = 0x0f040f0ful; /* MSVW03 */
	msvw[MSVW_I42].w1 = 0x04040f0ful; /* MSVW04 */
	msvw[MSVW_I47].w1 = 0x0404040ful; /* MSVW07 */

	MEC17XX_INT_ENABLE(24) = 0xfff3b177ul;
	MEC17XX_INT_ENABLE(25) = 0x01ul;

	MEC17XX_INT_SOURCE(24) = 0xfffffffful;
	MEC17XX_INT_SOURCE(25) = 0xfffffffful;
#endif

	MEC17XX_INT_BLK_EN = (1ul << 24) + (1ul << 25);

	task_enable_irq(MEC17XX_IRQ_GIRQ24);
	task_enable_irq(MEC17XX_IRQ_GIRQ25);

	CPRINTS("eSPI VW Pre-Init Done");
	TRACE0(31, ESPI, 0, "eSPI VW Pre-Init Done");
}


/*
 * If VWire, Flash, and OOB channels have been enabled 
 * then set VWires SLAVE_BOOT_LOAD_STATUS = SLAVE_BOOT_LOAD_DONE = 1
 * SLAVE_BOOT_LOAD_STATUS = SRC3 of Slave-to-Master Index 05h
 * SLAVE_BOOT_LOAD_DONE = SRC0 of Slave-to-Master Index 05h
 * Note, if set individually then set status first then done.
 * We set both simultaneously. ESPI_ALERT# will assert only if one 
 * or both bits change.
 * SRC0 is bit[32] of SMVW01 
 * SRC3 is bit[56] of SMVW01
 */
static void espi_send_boot_load_done(void)
{
	volatile struct mec17xx_espi_smvw *smvw;

	smvw = (volatile struct mec17xx_espi_smvw *)(MEC17XX_ESPI_SMVW_BASE);

	/* Set SLAVE_BOOT_LOAD_STATUS = SLAVE_BOOT_LOAD_DONE = 1 */
	smvw[SMVW_I05].w1 |= (1ul << 24) + (1ul << 0);
	CPRINTS("eSPI Send SLAVE_BOOT_LOAD_STATUS = SLAVE_BOOT_LOAD_DONE = 1");
	TRACE0(32, ESPI, 0, "Send VW SLAVE_BOOT_LOAD_STATUS = SLAVE_BOOT_LOAD_DONE = 1");
}


/*
 * Clear HOST_RST_WARN# (M2S 07h:0) = 0
 * Clear HOST_C10 (M2S 47h:0) = 0
 * Clear HOST_RST_ACK# (S2M 06h:3) = 0
 * Set SCI# (S2M 06h:0), SMI# (S2M 06h:1), RCIN# (S2M 06h:2) = 1
 */
#if 0
static void espi_vw_after_pltrst(void)
{
	volatile struct mec17xx_espi_msvw *msvw;
	volatile struct mec17xx_espi_smvw *smvw;

	msvw = (volatile struct mec17xx_espi_msvw *)(MEC17XX_ESPI_MSVW_BASE);
	msvw[MSVW_I07].src0 = 0;
	msvw[MSVW_I47].src0 = 0;

	smvw = (volatile struct mec17xx_espi_smvw *)(MEC17XX_ESPI_SMVW_BASE);
	smvw[SMVW_I06].w1 = (1ul << 0) + (1ul << 8) + (1ul << 16) + (0ul << 24);
}
#endif

/*
 * Clear SUS_STAT# (M2S 03h:0), PLTRST# (M2S 03h:1), OOB_RST_WARN (M2S 03h:2) = 0
 * Clear SUS_WARN# (M2S 41h:0), SUS_PWRDN_ACK (M2S 41h:1), SLP_A# ((M2S 41h:3) = 0
 * Clear OOB_RST_ACK# (S2M 04h:0) = 0
 * Set WAKE# (S2M 04h:2), PME# (S2M 04h:3) = 1
 * Clear SLAVE_BOOT_LOAD_DONE (S2M 05h:0), ERROR_FATAL (S2M 05h:1),
 *	ERROR_NONFATAL (S2M 05h:2), SLAVE_BOOT_LOAD_STATUS (S2M 05h:3) = 0
 * Clear SUS_ACK# (S2M 40h:0) = 0
 * call espi_vw_after_pltrst()
 */
#if 0
static void espi_vw_init_after_espi_reset(void)
{
	volatile struct mec17xx_espi_msvw *msvw;
	volatile struct mec17xx_espi_smvw *smvw;

	msvw = (volatile struct mec17xx_espi_msvw *)(MEC17XX_ESPI_MSVW_BASE);
	msvw[MSVW_I03].w2 = (0ul << 0) + (0ul << 8) + (0ul << 16);
	msvw[MSVW_I41].w2 = (0ul << 0) + (0ul << 8) + (0ul << 24);

	smvw = (volatile struct mec17xx_espi_smvw *)(MEC17XX_ESPI_SMVW_BASE);
	smvw[SMVW_I04].w1 = (0ul << 0) + (1ul << 16) + (1ul << 24);
	smvw[SMVW_I05].w1 = (0ul << 0) + (0ul << 8) + (0ul << 16) + (0ul << 24);
	smvw[SMVW_I40].src0 = 0;

	espi_vw_after_pltrst();
}
#endif


#if 0
/*
 * one function to program
 * IOBase = 16 bits
 * IOMask = 8 bits
 *
 * one funnction to program
 * Membase = 64 bits
 * MemMask = 8 bits
 *
 * one funtion to set valid
 *
 * one function for interrupt control
 *
 * Is it worth it? No.
 */
void ldn_cfg_acpi_ec(uint8_t instance, uint32_t iobase_mask, uint32_t membase)
{
	/* Set up ACPI0 for 0x62/0x66 */
	MEC17XX_ESPI_IO_BAR(MEC17XX_ESPI_IO_BAR_ID_ACPI_EC0) =
			(0x62ul << 16) + 0x01ul;
	MEC17XX_INT_ENABLE(MEC17XX_ACPI_EC_GIRQ) = MEC17XX_ACPI_EC_IBF_GIRQ_BIT(0);
	/* Clear STATUS_PROCESSING bit in case it was set during sysjump */
	MEC17XX_ACPI_EC_STATUS(0) &= ~EC_LPC_STATUS_PROCESSING;
	task_enable_irq(MEC17XX_IRQ_ACPIEC0_IBF);
}
#endif

/*
 * Called when eSPI PLTRST# VWire de-asserts
 * Re-initialize any hardware that was reset while PLTRST# was
 * asserted.
 * Logical Device BAR's, etc.
 *   Each BAR requires address, mask, and valid bit
 *     mask = bit map of address[7:0] to mask out
 *	0 = no masking, match exact address
 *	0x01 = mask bit[0], match two consecutive addresses
 *	0xff = mask bits[7:0], match 256 consecutive bytes
 *     eSPI has two registers for each BAR
 *       Host visible register
 *		base address in bits[31:16]
 *		valid = bit[0]
 *       EC only register
 *		mask = bits[7:0]
 *		Logical device number = bits[13:8]
 *		Virtualized = bit[16] Not Implemented
 */
#if 1
static void espi_host_init(void)
{
	CPRINTS("eSPI - espi_host_init");
	TRACE0(33, ESPI, 0, "eSPI Host Init");

	/* BAR's */

	/* Configuration IO BAR set to 0x2E/0x2F */
	MEC17XX_ESPI_IO_BAR_CTL_MASK(MEC17XX_ESPI_IO_BAR_ID_CFG_PORT) = 0x01;
	MEC17XX_ESPI_IO_BAR_ADDR_LSB(MEC17XX_ESPI_IO_BAR_ID_CFG_PORT) = 0x2E;
	MEC17XX_ESPI_IO_BAR_ADDR_MSB(MEC17XX_ESPI_IO_BAR_ID_CFG_PORT) = 0x00;
	MEC17XX_ESPI_IO_BAR_VALID(MEC17XX_ESPI_IO_BAR_ID_CFG_PORT) = 1;

	/* Set up ACPI0 for 0x62/0x66 */
	MEC17XX_ESPI_IO_BAR_CTL_MASK(MEC17XX_ESPI_IO_BAR_ID_ACPI_EC0) = 0x04;
	MEC17XX_ESPI_IO_BAR(MEC17XX_ESPI_IO_BAR_ID_ACPI_EC0) =
			(0x62ul << 16) + 0x01ul;
	MEC17XX_INT_ENABLE(MEC17XX_ACPI_EC_GIRQ) = MEC17XX_ACPI_EC_IBF_GIRQ_BIT(0);
	/* Clear STATUS_PROCESSING bit in case it was set during sysjump */
	MEC17XX_ACPI_EC_STATUS(0) &= ~EC_LPC_STATUS_PROCESSING;
	task_enable_irq(MEC17XX_IRQ_ACPIEC0_IBF);

	/* Set up ACPI1 for 0x200-0x203, 0x204-0x207 */
	MEC17XX_ESPI_IO_BAR_CTL_MASK(MEC17XX_ESPI_IO_BAR_ID_ACPI_EC1) = 0x07;
	MEC17XX_ESPI_IO_BAR(MEC17XX_ESPI_IO_BAR_ID_ACPI_EC1) =
			(0x200ul << 16) + 0x01ul;
	MEC17XX_INT_ENABLE(MEC17XX_ACPI_EC_GIRQ) = MEC17XX_ACPI_EC_IBF_GIRQ_BIT(1);
	MEC17XX_ACPI_EC_STATUS(1) &= ~EC_LPC_STATUS_PROCESSING;
	task_enable_irq(MEC17XX_IRQ_ACPIEC1_IBF);

	/* Set up 8042 interface at 0x60/0x64 */
	MEC17XX_ESPI_IO_BAR_CTL_MASK(MEC17XX_ESPI_IO_BAR_ID_8042) = 0x04;
	MEC17XX_ESPI_IO_BAR(MEC17XX_ESPI_IO_BAR_ID_8042) =
			(0x64ul << 16) + 0x01ul;

	/* Set up indication of Auxillary sts */
	MEC17XX_8042_KB_CTRL |= 1 << 7;

	MEC17XX_8042_ACT |= 1;
	MEC17XX_INT_ENABLE(MEC17XX_8042_GIRQ) = MEC17XX_8042_OBE_GIRQ_BIT +
			MEC17XX_8042_IBF_GIRQ_BIT;
	task_enable_irq(MEC17XX_IRQ_8042EM_IBF);
	task_enable_irq(MEC17XX_IRQ_8042EM_OBF);

#ifndef CONFIG_KEYBOARD_IRQ_GPIO
	/* Set up SERIRQ for keyboard */
	MEC17XX_8042_KB_CTRL |= (1 << 5);
	MEC17XX_ESPI_IO_SERIRQ_REG(MEC17XX_ESPI_8042_SIRQ0) = 1;
	// MEC17XX_ESPI_IO_SERIRQ_REG(MEC17XX_ESPI_8042_SIRQ1) = 12;
#endif

	/* Set up EMI module for memory mapped region,
	 * IO range 0x800-0x80f */
	MEC17XX_ESPI_IO_BAR_CTL_MASK(MEC17XX_ESPI_IO_BAR_ID_EMI0) = 0x0F;
	MEC17XX_ESPI_IO_BAR(MEC17XX_ESPI_IO_BAR_ID_EMI0) =
			(0x800ul << 16) + 0x01ul;
	MEC17XX_INT_ENABLE(MEC17XX_EMI_GIRQ) = MEC17XX_EMI_GIRQ_BIT(0);
	task_enable_irq(MEC17XX_IRQ_EMI0);


	/* Access data RAM
	 * MEC17xx EMI Base address register = physical address in SRAM of buffer.
	 * EMI hardware adds 16-bit offset Host programs into EC_Address_LSB/MSB
	 * registers.
	 */
	MEC17XX_EMI_MBA0(0) = lpc_mem_mapped_addr();

	/*
	 * Limit EMI read / write range. First 256 bytes are RW for host
	 * commands. Second 256 bytes are RO for mem-mapped data.
	 */
	MEC17XX_EMI_MRL0(0) = 0x200;
	MEC17XX_EMI_MWL0(0) = 0x100;

#if 0
	/* Set up Mailbox for Port80 trapping */
	MEC17XX_MBX_INDEX = 0xff;
	MEC17XX_ESPI_IO_BAR(MEC17XX_ESPI_IO_BAR_ID_MAILBOX) =
			(0x80ul << 16) + 0x01ul;
#else
	/* Setup Port80 Debug Hardware for I/O 80h */
	MEC17XX_P80_CFG(0) = MEC17XX_P80_FLUSH_FIFO_WO +
			MEC17XX_P80_RESET_TIMESTAMP_WO;

	/* IO 0x80 only, mask = 0 */
	MEC17XX_ESPI_IO_BAR_CTL_MASK(MEC17XX_ESPI_IO_BAR_P80_0) = 0x00;
	MEC17XX_ESPI_IO_BAR(MEC17XX_ESPI_IO_BAR_P80_0) =
			(0x80ul << 16) + 0x01ul;

	MEC17XX_P80_CFG(0) = MEC17XX_P80_FIFO_THRHOLD_1 +
			MEC17XX_P80_TIMEBASE_1500KHZ +
			MEC17XX_P80_TIMER_ENABLE;


	MEC17XX_P80_ACTIVATE(0) = 1;

	MEC17XX_INT_SOURCE(15) = (1ul << 22);
	MEC17XX_INT_ENABLE(15) = (1ul << 22);

	task_enable_irq(MEC17XX_IRQ_PORT80DBG0);

#endif
	lpc_mem_mapped_init();

	/* TODO VWires init on PLTRST# de-assertion */
	

	MEC17XX_ESPI_PC_STATUS = 0xfffffffful;
	/* PC enable & Mastering enable changes */
	MEC17XX_ESPI_PC_IEN = (1ul << 25) + (1ul << 28);



	/* Sufficiently initialized */
	/* TODO local global in lpc.c */
	// TODO init_done = 1;
	lpc_set_init_done(1);

	/* last set eSPI Peripheral Channel Ready = 1 */
	/* Done in ISR for PC Channel */
	MEC17XX_ESPI_IO_PC_READY = 1;


#if 1
	/* Update host events now that we can copy them to memmap */
	/* TODO local function in lpc.c 
	 * This routine may pulse SCI# and/or SMI#
	 * For eSPI these are virtual wires. VWire channel should be 
	 * enabled before PLTRST# is de-asserted so its safe BUT has 
	 * PC Channel Enable occured? 
	*/
	lpc_update_host_event_status();
#endif
	CPRINTS("eSPI - espi_host_init Done");
	TRACE0(34, ESPI, 0, "eSPI Host Init Done");
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, espi_host_init, HOOK_PRIO_FIRST);
#endif


static void espi_oob_flush(void)
{
	/* TODO Flush OOB TX in response to VWire OOB_RST_WARN == 1 */
}


static void espi_pc_flush(void)
{
	/* TODO Flush Peripheral channel in response to VWire
	 * HOST_RST_WARN == 1 */
}

/* The ISRs of VW signals which used for power sequences */
void espi_vw_power_signal_interrupt(enum espi_vw_signal signal)
{
	/* TODO: Add VW handler in power/common.c */
	CPRINTS("eSPI power signal interrupt for VW %d",signal);
	TRACE1(35, ESPI, 0, "eSPI power signal interrupt for VW %d",(signal - VW_SIGNAL_BASE));
	power_signal_interrupt((enum gpio_signal) signal);
}

/*****************************************************************************/
/* IC specific low-level driver */


/**
 * Set eSPI Virtual-Wire signal to Host
 *
 * @param signal vw signal needs to set
 * @param level  level of vw signal
 * @return EC_SUCCESS, or non-zero if error.
 */
int espi_vw_set_wire(enum espi_vw_signal signal, uint8_t level)
{
	uint8_t tidx, ridx, src_num;
	volatile struct mec17xx_espi_smvw *smvw;

	tidx = espi_vw_get_signal_index(signal);

	if (tidx < 0) {
		return EC_ERROR_PARAM1;
	}

	if (0 == (vw_info_tbl[tidx].flags & (1u << 0))) {
		return EC_ERROR_PARAM1; /* signal is Master-to-Slave */
	}

	ridx = vw_info_tbl[tidx].reg_idx;
	src_num = vw_info_tbl[tidx].src_num;

	smvw = (volatile struct mec17xx_espi_smvw *)(MEC17XX_ESPI_SMVW_BASE);

	if (level) {
		level = 1;
	}

	if (VW_SLAVE_BTLD_STATUS_DONE == signal) {
		smvw[ridx].src3 = level; /* SLAVE_BOOT_LOAD_STATUS */
		smvw[ridx].src0 = level; /* SLAVE_BOOT_LOAD_DONE after status */
	} else {
		smvw[ridx].src[src_num] = level;
		level = smvw[ridx].src[src_num];
	}

#if DEBUG_ESPI
	CPRINTS("eSPI VW Set Wire %s = %d",vw_info_tbl[tidx].name_str, level);
	TRACE2(36, ESPI, 0, "eSPI VW Set Wire [%d] = %d",(signal - VW_SIGNAL_BASE),level);
#endif

	return EC_SUCCESS;
}

/*
 * TODO Create a pulse on a Slave-to-Master VWire
 * Use case is generate low pulse on SCI# virtual wire.
 * TODO - Add a timeout mechanism because we are waiting on
 * Host eSPI Master to respond to eSPI Alert.
 */
int espi_vw_pulse_wire(enum espi_vw_signal signal)
{
	uint8_t tidx, ridx, src_num, level;
	volatile struct mec17xx_espi_smvw *smvw;

	tidx = espi_vw_get_signal_index(signal);

	if (tidx < 0) {
		return EC_ERROR_PARAM1;
	}

	if (0 == (vw_info_tbl[tidx].flags & (1u << 0))) {
		return EC_ERROR_PARAM1; /* signal is Master-to-Slave */
	}


	ridx = vw_info_tbl[tidx].reg_idx;
	src_num = vw_info_tbl[tidx].src_num;

	smvw = (volatile struct mec17xx_espi_smvw *)(MEC17XX_ESPI_SMVW_BASE);

	/* invert current level */
	level = smvw[ridx].src[src_num];

#if DEBUG_ESPI
	CPRINTS("eSPI VW Pulse Wire %s",vw_info_tbl[tidx].name_str);
	TRACE2(37, ESPI, 0, "eSPI VW Pulse Wire [%d] = %d",(signal - VW_SIGNAL_BASE),(~level & 0x01));
#endif

	smvw[ridx].src[src_num] = ~level;
	/* wait for Master to read the VWire index */
	while (smvw[ridx].change & (1u << src_num));
	smvw[ridx].src[src_num] = level;
	/* wait for Master to read the VWire index */
	while (smvw[ridx].change & (1u << src_num));

	return EC_SUCCESS;
}

/**
 * Get eSPI Virtual-Wire signal from host
 *
 * @param signal vw signal needs to get
 * @return      1: set by host, otherwise: no signal
 */
int espi_vw_get_wire(enum espi_vw_signal signal)
{
	int vw;
	uint8_t tidx, ridx, src_num;
	volatile struct mec17xx_espi_msvw *msvw;

	vw = 0;

	tidx = espi_vw_get_signal_index(signal);

	if (tidx >= 0) {
		if (0 == (vw_info_tbl[tidx].flags & (1u << 0))) {
			ridx = vw_info_tbl[tidx].reg_idx;
			src_num = vw_info_tbl[tidx].src_num;
			msvw = (volatile struct mec17xx_espi_msvw *)(MEC17XX_ESPI_MSVW_BASE);
			vw = (int)(msvw[ridx].src[src_num] & 0x01);
#if DEBUG_ESPI
			CPRINTS("eSPI VW Get Wire %s = %d",vw_info_tbl[tidx].name_str, vw);
			TRACE2(38, ESPI, 0, "eSPI VW Get Wire [%d] = %d",(signal - VW_SIGNAL_BASE),vw);
#endif
		}
	}

	return vw;
}

/**
 * Enable VW interrupt of power sequence signal
 *
 * @param signal vw signal needs to enable interrupt
 * @return EC_SUCCESS, or non-zero if error.
 */
int espi_vw_enable_wire_int(enum espi_vw_signal signal)
{
	uint8_t tidx, ridx, src_num, girq_num, bpos;
	volatile struct mec17xx_espi_msvw *msvw;

	tidx = espi_vw_get_signal_index(signal);

	if (tidx < 0) {
		return EC_ERROR_PARAM1;
	}

	if (0 != (vw_info_tbl[tidx].flags & (1u << 0))) {
		return EC_ERROR_PARAM1; /* signal is Slave-to-Master */
	}

	CPRINTS("eSPI VW Enable Interrupt API called for %s",vw_info_tbl[tidx].name_str);
	TRACE1(39, ESPI, 0, "eSPI VW Enable Interrupt for VW[%d]",(signal - VW_SIGNAL_BASE));

	ridx = vw_info_tbl[tidx].reg_idx;
	src_num = vw_info_tbl[tidx].src_num;

	/*
	 * Set SRCn_IRQ_SELECT field for VWire to either edge
	 * Write enable set bit in GIRQ24 or GIRQ25
	 * GIRQ24 MSVW00[0:3] through MSVW06[0:3] (bits[0:27])
	 * GIRQ25 MSVW07[0:3] through MSVW10[0:3] (bits[0:25])
	 */
	msvw = (volatile struct mec17xx_espi_msvw *)(MEC17XX_ESPI_MSVW_BASE);

	msvw[ridx].src_irq_sel[src_num] = MEC17XX_ESPI_MSVW_IRQSEL_BOTH_EDGES;

	girq_num = 24;
	if (ridx > 6) {
		girq_num++;
		ridx -= 7;
	}
	bpos = (ridx << 2) + src_num;

	MEC17XX_INT_SOURCE(girq_num) = (1ul << bpos);
	MEC17XX_INT_ENABLE(girq_num) = (1ul << bpos);


	return EC_SUCCESS;
}

/**
 * Disable VW interrupt of power sequence signal
 *
 * @param signal vw signal needs to disable interrupt
 * @return EC_SUCCESS, or non-zero if error.
 */
int espi_vw_disable_wire_int(enum espi_vw_signal signal)
{
	uint8_t tidx, ridx, src_num, bpos;
	// volatile struct mec17xx_espi_msvw *msvw;

	tidx = espi_vw_get_signal_index(signal);

	if (tidx < 0) {
		return EC_ERROR_PARAM1;
	}

	if (0 != (vw_info_tbl[tidx].flags & (1u << 0))) {
		return EC_ERROR_PARAM1; /* signal is Slave-to-Master */
	}

	CPRINTS("eSPI VW Disable Interrupt API called for %s",vw_info_tbl[tidx].name_str);
	TRACE1(40, ESPI, 0, "eSPI VW Disable Interrupt for VW[%d]",(signal - VW_SIGNAL_BASE));

	ridx = vw_info_tbl[tidx].reg_idx;
	src_num = vw_info_tbl[tidx].src_num;

	/*
	 * Set SRCn_IRQ_SELECT field for VWire to either edge
	 * Write enable set bit in GIRQ24 or GIRQ25
	 * GIRQ24 MSVW00[0:3] through MSVW06[0:3] (bits[0:27])
	 * GIRQ25 MSVW07[0:3] through MSVW10[0:3] (bits[0:25])
	 */
	if (ridx < 7) {
		bpos = (ridx << 2) + src_num;
		MEC17XX_INT_DISABLE(24) = (1ul << bpos);

	} else {
		bpos = ((ridx - 7) << 2) + src_num;
		MEC17XX_INT_DISABLE(25) = (1ul << bpos);
	}

	return EC_SUCCESS;
}

/*****************************************************************************/
/* VW event handlers */

#ifdef CONFIG_CHIPSET_RESET_HOOK
static void espi_chipset_reset(void)
{
	hook_notify(HOOK_CHIPSET_RESET);
}
DECLARE_DEFERRED(espi_chipset_reset);
#endif


/* SLP_Sx event handler */
void espi_vw_evt_slp_s3_n(uint32_t wire_state, uint32_t bpos)
{
	CPRINTS("VW SLP_S3: %d", wire_state);
	TRACE1(41, ESPI, 0, "VW_SLP_S3_L change to %d",wire_state);
	espi_vw_power_signal_interrupt(VW_SLP_S3_L);
}

void espi_vw_evt_slp_s4_n(uint32_t wire_state, uint32_t bpos)
{
	CPRINTS("VW SLP_S4: %d", wire_state);
	TRACE1(42, ESPI, 0, "VW_SLP_S4_L change to %d",wire_state);
	espi_vw_power_signal_interrupt(VW_SLP_S4_L);
}

void espi_vw_evt_slp_s5_n(uint32_t wire_state, uint32_t bpos)
{
	CPRINTS("VW SLP_S5: %d", wire_state);
	TRACE1(43, ESPI, 0, "VW_SLP_S5_L change to %d",wire_state);
	espi_vw_power_signal_interrupt(VW_SLP_S5_L);
}

void espi_vw_evt_sus_stat_n(uint32_t wire_state, uint32_t bpos)
{
	CPRINTS("VW SUS_STAT: %d", wire_state);
	TRACE1(44, ESPI, 0, "VW_SUS_STAT change to %d",wire_state);
	espi_vw_power_signal_interrupt(VW_SUS_STAT_L);
}

/* PLTRST# event handler */
void espi_vw_evt_pltrst_n(uint32_t wire_state, uint32_t bpos)
{
	CPRINTS("VW PLTRST#: %d", wire_state);
	TRACE1(45, ESPI, 0, "VW_PLTRST# change to %d",wire_state);

	if (wire_state) { /* Platform Reset de-assertion */
		espi_host_init();
	} else { /* assertion */
#ifdef CONFIG_CHIPSET_RESET_HOOK
		hook_call_deferred(&espi_chipset_reset_data, MSEC);
#endif
	}
}

/* OOB Reset Warn event handler */
void espi_vw_evt_oob_rst_warn(uint32_t wire_state, uint32_t bpos)
{
	CPRINTS("VW OOB_RST_WARN: %d", wire_state);
	TRACE1(46, ESPI, 0, "VW_OOB_RST_WARN change to %d",wire_state);

	/* TODO - if OOB_RST_WARN == 1 then flush OOB and idle OOB channel upstream requests */
	espi_oob_flush();

	espi_vw_set_wire(VW_OOB_RST_ACK, wire_state);
}

/* SUS_WARN# event handler */
void espi_vw_evt_sus_warn_n(uint32_t wire_state, uint32_t bpos)
{
	CPRINTS("VW SUS_WARN#: %d", wire_state);
	TRACE1(47, ESPI, 0, "VW_SUS_WARN# change to %d",wire_state);

	udelay(100);

	/* TODO - Add any Deep Sx prep here
	 * NOTE: we could schedule a deferred function and have
	 * it send ACK to host after preparing for Deep Sx
	*/

	/* Send ACK to host by WARN#'s wire */
	espi_vw_set_wire(VW_SUS_ACK, wire_state);
}

/* SUS_PWRDN_ACK
 * PCH is informing us it does not need suspend power well.
 * if SUS_PWRDN_ACK == 1 we can turn off suspend power well assuming
 * hardware design allow.
 */
void espi_vw_evt_sus_pwrdn_ack(uint32_t wire_state, uint32_t bpos)
{
	TRACE1(48, ESPI, 0, "VW_SUS_PWRDN_ACK change to %d",wire_state);
	CPRINTS("VW SUS_PWRDN_ACK: %d", wire_state);
}

/* SLP_A#(SLP_M#) */
void espi_vw_evt_slp_a_n(uint32_t wire_state, uint32_t bpos)
{
	CPRINTS("VW SLP_A: %d", wire_state);
	TRACE1(49, ESPI, 0, "VW_SLP_A# change to %d",wire_state);

	/* TODO - Handle ASW well devices if any */
}

/* HOST_RST WARN event handler */
void espi_vw_evt_host_rst_warn(uint32_t wire_state, uint32_t bpos)
{
	CPRINTS("VW HOST_RST_WARN: %d", wire_state);
	TRACE1(50, ESPI, 0, "VW_HOST_RST_WARN change to %d",wire_state);

	/* TODO - If HOST_RST_WARN = 1 then we must flush and idle Peripheral Channel */
	espi_pc_flush();

	/* Send HOST_RST_ACK to host */
	espi_vw_set_wire(VW_HOST_RST_ACK, wire_state);
}

/* SLP_LAN# */
void espi_vw_evt_slp_lan_n(uint32_t wire_state, uint32_t bpos)
{
	CPRINTS("VW SLP_LAN: %d", wire_state);
	TRACE1(51, ESPI, 0, "VW_SLP_LAN# change to %d",wire_state);
}

/* SLP_WLAN# */
void espi_vw_evt_slp_wlan_n(uint32_t wire_state, uint32_t bpos)
{
	CPRINTS("VW SLP_WLAN: %d", wire_state);
	TRACE1(52, ESPI, 0, "VW_SLP_WLAN# change to %d",wire_state);
}

void espi_vw_evt_host_c10(uint32_t wire_state, uint32_t bpos)
{
	CPRINTS("VW HOST_C10: %d", wire_state);
	TRACE1(53, ESPI, 0, "VW_HOST_C10 change to %d",wire_state);
}

void espi_vw_evt1_dflt(uint32_t wire_state, uint32_t bpos)
{
	CPRINTS("Unknown eSPI M2S VW: state = %d  GIRQ24 bitpos = %d", wire_state,bpos);
	TRACE2(54, ESPI, 0, "eSPI Unknown M2S VW change: GIRQ24 bitpos=%d state=%d",bpos,wire_state);
	MEC17XX_INT_DISABLE(24) = (1ul << bpos);
}

void espi_vw_evt2_dflt(uint32_t wire_state, uint32_t bpos)
{
	CPRINTS("Unknown eSPI M2S VW: state = %d  GIRQ25 bitpos = %d", wire_state,bpos);
	TRACE2(55, ESPI, 0, "eSPI Unknown M2S VW change: GIRQ25 bitpos=%d state=%d",bpos,wire_state);
	MEC17XX_INT_DISABLE(25) = (1ul << bpos);
}

/*****************************************************************************/
/* Interrupt handlers */

/* MEC1701H
 * GIRQ19 all direct connect capable, none wake capable
 * 	b[0] = Peripheral Channel (PC)
 * 	b[1] = Bus Master 1 (BM1)
 * 	b[2] = Bus Master 2 (BM2)
 * 	b[3] = LTR
 * 	b[4] = OOB_UP
 * 	b[5] = OOB_DN
 * 	b[6] = Flash Channel (FC)
 * 	b[7] = ESPI_RESET# change
 * 	b[8] = VWire Channel (VW) enable assertion
 * 	b[9:31] = 0 reserved
 *
 * GIRQ22 b[9]=ESPI interface wake peripheral logic only, not EC.
 * 	Not direct connect capable
 *
 * GIRQ24
 *	b[0:3]   = MSVW00_SRC[0:3]
 * 	b[4:7]   = MSVW01_SRC[0:3]
 * 	b[8:11]  = MSVW02_SRC[0:3]
 * 	b[12:15] = MSVW03_SRC[0:3]
 * 	b[16:19] = MSVW04_SRC[0:3]
 * 	b[20:23] = MSVW05_SRC[0:3]
 * 	b[24:27] = MSVW06_SRC[0:3]
 * 	b[28:31] = 0 reserved
 *
 * GIRQ25
 * 	b[0:3]   = MSVW07_SRC[0:3]
 * 	b[4:7]   = MSVW08_SRC[0:3]
 * 	b[8:11]  = MSVW09_SRC[0:3]
 * 	b[12:15] = MSVW10_SRC[0:3]
 * 	b[16:31] = 0 reserved
 *
 */

typedef void (*FPVW)(uint32_t, uint32_t);

#define MEC17XX_GIRQ24_NUM_M2S	(7 * 4)
const FPVW girq24_vw_handlers[MEC17XX_GIRQ24_NUM_M2S] = {
	espi_vw_evt_slp_s3_n,	/* MSVW00, Host M2S 02h */
	espi_vw_evt_slp_s4_n,
	espi_vw_evt_slp_s5_n,
	espi_vw_evt1_dflt,
	espi_vw_evt_sus_stat_n,	/* MSVW01, Host M2S 03h */
	espi_vw_evt_pltrst_n,
	espi_vw_evt_oob_rst_warn,
	espi_vw_evt1_dflt,
	espi_vw_evt_host_rst_warn, /* MSVW02, Host M2S 07h */
	espi_vw_evt1_dflt,
	espi_vw_evt1_dflt,
	espi_vw_evt1_dflt,
	espi_vw_evt_sus_warn_n,	/* MSVW03, Host M2S 41h */
	espi_vw_evt_sus_pwrdn_ack,
	espi_vw_evt1_dflt,
	espi_vw_evt_slp_a_n,
	espi_vw_evt_slp_lan_n,	/* MSVW04, Host M2S 42h */
	espi_vw_evt_slp_wlan_n,
	espi_vw_evt1_dflt,
	espi_vw_evt1_dflt,
	espi_vw_evt1_dflt,	/* MSVW05, Host M2S 43h */
	espi_vw_evt1_dflt,
	espi_vw_evt1_dflt,
	espi_vw_evt1_dflt,
	espi_vw_evt1_dflt,	/* MSVW06, Host M2S 44h */
	espi_vw_evt1_dflt,
	espi_vw_evt1_dflt,
	espi_vw_evt1_dflt
};

#define MEC17XX_GIRQ25_NUM_M2S	(4 * 4)
const FPVW girq25_vw_handlers[MEC17XX_GIRQ25_NUM_M2S] = {
	espi_vw_evt_host_c10,	/* MSVW07, Host M2S 47h */
	espi_vw_evt2_dflt,
	espi_vw_evt2_dflt,
	espi_vw_evt2_dflt,
	espi_vw_evt2_dflt,	/* MSVW08 unassigned */
	espi_vw_evt2_dflt,
	espi_vw_evt2_dflt,
	espi_vw_evt2_dflt,
	espi_vw_evt2_dflt,	/* MSVW09 unassigned */
	espi_vw_evt2_dflt,
	espi_vw_evt2_dflt,
	espi_vw_evt2_dflt,
	espi_vw_evt2_dflt,	/* MSVW10 unassigned */
	espi_vw_evt2_dflt,
	espi_vw_evt2_dflt,
	espi_vw_evt2_dflt,
};

/* Interrupt handler for eSPI virtual wires in MSVW00 - MSVW01 */
void espi_mswv1_interrupt(void)
{
	uint32_t d, girq24_result, bpos;

	d = MEC17XX_INT_ENABLE(24);
	girq24_result = MEC17XX_INT_RESULT(24);
	MEC17XX_INT_SOURCE(24) = girq24_result;

	bpos = __builtin_ctz(girq24_result); /* rbit, clz sequence */
	while (32 != bpos) {
		d = *(uint8_t *)(MEC17XX_ESPI_MSVW_BASE + 8 + (12 * (bpos >> 2)) + (bpos & 0x03)) & 0x01;
		(girq24_vw_handlers[bpos])(d, bpos);
		girq24_result &= ~(1ul << bpos);
		bpos = __builtin_ctz(girq24_result);
	}
}
DECLARE_IRQ(MEC17XX_IRQ_GIRQ24, espi_mswv1_interrupt, 2);


/* Interrupt handler for eSPI virtual wires in MSVW07 - MSVW10 */
void espi_msvw2_interrupt(void)
{
	uint32_t d, girq25_result, bpos;

	d = MEC17XX_INT_ENABLE(25);
	girq25_result = MEC17XX_INT_RESULT(25);
	MEC17XX_INT_SOURCE(25) = girq25_result;

	bpos = __builtin_ctz(girq25_result); /* rbit, clz sequence */
	while (32 != bpos) {
		d = *(uint8_t *)(MEC17XX_ESPI_MSVW_BASE + (12 * 7) + 8 + (12 * (bpos >> 2)) + (bpos & 0x03)) & 0x01;
		(girq25_vw_handlers[bpos])(d, bpos);
		girq25_result &= ~(1ul << bpos);
		bpos = __builtin_ctz(girq25_result);
	}
}
DECLARE_IRQ(MEC17XX_IRQ_GIRQ25, espi_msvw2_interrupt, 2);



/*
 * NOTES:
 * While ESPI_RESET# is asserted, all eSPI blocks are held in reset and
 * their registers can't be programmed. All channel Enable and Ready bits
 * are cleared. The only operational logic is the ESPI_RESET# change
 * detection logic.
 * Once ESPI_RESET# de-asserts, firmware can enable interrupts on all other
 * eSPI channels/components.
 * Implications are:
 * ESPI_RESET# assertion -
 * 	All channel ready bits are cleared stopping all outstanding transactions
 * 	and clearing registers and internal FIFO's.
 * ESPI_RESET# de-assertion -
 * 	All channels/components can now be programmed and can detect reception
 * 	of channel enable messages from the eSPI Master.
 */

/*
 * eSPI Reset change handler
 * Multiple scenarios must be handled.
 * eSPI Link initialization from de-assertion of RSMRST#
 *	Upon RSMRST# de-assertion, the PCH may drive ESPI_RESET# low
 *	and then back high. If the platform has a pull-down on ESPI_RESET#
 *	then we will not see both edges. We must handle the scenario where
 *	ESPI_RESET# has only a rising edge or is pulsed low once RSMRST#
 *	has been released.
 * eSPI Link is operational and PCH asserts ESPI_RESET# due to global reset
 * event or some other system problem.
 *	eSPI link is operational and the system generates a global reset event
 *	to the PCH. EC is unaware of global reset and sees PCH activate
 *	ESPI_RESET#.
 *
 * ESPI_RESET# assertion will disable all MEC17xx eSPI channel ready bits place
 * all channels is reset state.  Any hardware affected by ESPI_RESET# must be
 * re-initialized after ESPI_RESET# de-asserts.
 *
 * Note ESPI_RESET# is not equivalent to LPC LRESET#. LRESET# is equivalent to
 * eSPI Platform Reset.
 *
 */
void espi_reset_isr(void)
{
	uint8_t erst;

	erst = MEC17XX_ESPI_IO_RESET_STATUS;
	MEC17XX_ESPI_IO_RESET_STATUS = erst;
	MEC17XX_INT_SOURCE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_RESET_GIRQ_BIT;
	if (erst & (1ul << 1)) { /* rising edge - reset de-asserted */
		/* Arm interrupt detection of VW, FC, and OOB Enable messages
		 * PC is enabled by HW when PC Enable message is received
		 * Assumes NVIC has been programmed.
		 */
		MEC17XX_INT_ENABLE(MEC17XX_ESPI_GIRQ) = (
				MEC17XX_ESPI_PC_GIRQ_BIT +
				MEC17XX_ESPI_OOB_TX_GIRQ_BIT +
				MEC17XX_ESPI_FC_GIRQ_BIT +
				MEC17XX_ESPI_VW_EN_GIRQ_BIT);
		MEC17XX_ESPI_OOB_TX_IEN = (1ul << 1);
		MEC17XX_ESPI_FC_IEN = (1ul << 1);
		MEC17XX_ESPI_PC_IEN = (1ul << 25);
		CPRINTS("eSPI Reset de-assert");
		TRACE0(56, ESPI, 0, "eSPI Reset de-assert");

	} else { /* falling edge - reset asserted */
		MEC17XX_INT_SOURCE(MEC17XX_ESPI_GIRQ) = (
					MEC17XX_ESPI_PC_GIRQ_BIT +
					MEC17XX_ESPI_OOB_TX_GIRQ_BIT +
					MEC17XX_ESPI_FC_GIRQ_BIT +
					MEC17XX_ESPI_VW_EN_GIRQ_BIT);
		MEC17XX_INT_DISABLE(MEC17XX_ESPI_GIRQ) = (
					MEC17XX_ESPI_PC_GIRQ_BIT +
					MEC17XX_ESPI_OOB_TX_GIRQ_BIT +
					MEC17XX_ESPI_FC_GIRQ_BIT +
					MEC17XX_ESPI_VW_EN_GIRQ_BIT);
		espi_channels_ready = 0;

		chipset_handle_espi_reset_assert();
		
		CPRINTS("eSPI Reset assert");
		TRACE0(57, ESPI, 0, "eSPI Reset assert");
	}
}
DECLARE_IRQ(MEC17XX_IRQ_ESPI_RESET, espi_reset_isr, 3);

/*
 * eSPI Virtual Wire channel enable handler
 * Must disable once VW Enable is set by eSPI Master
 */
void espi_vw_en_isr(void)
{
	MEC17XX_INT_DISABLE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_VW_EN_GIRQ_BIT;
	MEC17XX_INT_SOURCE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_VW_EN_GIRQ_BIT;

	MEC17XX_ESPI_IO_VW_READY = 1;

	espi_channels_ready |= (1ul << 0);

	CPRINTS("eSPI VW Enable received, set VW Ready");
	TRACE0(58, ESPI, 0, "VW Enable. Set VW Ready");

	if (0x03 == (espi_channels_ready & 0x03)) {
		espi_send_boot_load_done();
	}
}
DECLARE_IRQ(MEC17XX_IRQ_ESPI_VW_EN, espi_vw_en_isr, 2);


/*
 * eSPI OOB TX and OOB channel enable change interrupt handler
 */
void espi_oob_tx_isr(void)
{
	uint32_t sts;

	sts = MEC17XX_ESPI_OOB_TX_STATUS;
	MEC17XX_ESPI_OOB_TX_STATUS = sts;
	MEC17XX_INT_SOURCE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_OOB_TX_GIRQ_BIT;
	if (sts & (1ul << 1)) {
		/* Channel Enable change */
		if (sts & (1ul << 9)) { /* enable? */
			MEC17XX_ESPI_OOB_RX_LEN = 73;
			MEC17XX_ESPI_IO_OOB_READY = 1;
			espi_channels_ready |= (1ul << 2);
			CPRINTS("eSPI OOB_UP ISR: Detected Master enabled OOB Channel");
			TRACE0(59, ESPI, 0, "eSPI OOB_TX OOB Enable. Set OOB RX Len = 73 and OOB Ready");
		} else { /* no, disabled by Master */
			espi_channels_ready &= ~(1ul << 2);
			CPRINTS("eSPI OOB_UP ISR: Detected Master disabled OOB Channel");
			TRACE0(60, ESPI, 0, "eSPI OOB_TX OOB Disable");
		}
	} else {
		/* TODO handle OOB Up transmit status: done and/or errors */
		CPRINTS("eSPI OOB_UP status = 0x%x", sts);
		TRACE11(61, ESPI, 0, "eSPI OOB_TX Status = 0x%08x",sts);
	}
}
DECLARE_IRQ(MEC17XX_IRQ_ESPI_OOB_UP, espi_oob_tx_isr, 2);


/* eSPI OOB RX interrupt handler */
void espi_oob_rx_isr(void)
{
	uint32_t sts;

	sts = MEC17XX_ESPI_OOB_RX_STATUS;
	MEC17XX_ESPI_OOB_RX_STATUS = sts;
	MEC17XX_INT_SOURCE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_OOB_RX_GIRQ_BIT;
	/* TODO handle OOB Up transmit status: done and/or errors */
	CPRINTS("eSPI OOB_DN status = 0x%x", sts);
	TRACE11(62, ESPI, 0, "eSPI OOB_RX Status = 0x%08x",sts);
}
DECLARE_IRQ(MEC17XX_IRQ_ESPI_OOB_DN, espi_oob_rx_isr, 2);


/*
 * eSPI Flash Channel enable change and data transfer
 * interrupt handler
 */
void espi_fc_isr(void)
{
	uint32_t sts;

	sts = MEC17XX_ESPI_FC_STATUS;
	MEC17XX_ESPI_FC_STATUS = sts;
	MEC17XX_INT_SOURCE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_FC_GIRQ_BIT;
	if (sts & (1ul << 1)) {
		/* Channel Enable change */
		if (sts & (1ul << 0)) { /* enable? */
			MEC17XX_ESPI_IO_FC_READY = 1;
			espi_channels_ready |= (1ul << 1);
			CPRINTS("eSPI FC ISR: Detected Master enabled FC Channel");
			TRACE0(63, ESPI, 0, "eSPI FC Enable");
			if (0x03 == (espi_channels_ready & 0x03)) {
				espi_send_boot_load_done();
			}
		} else { /* no, disabled by Master */
			espi_channels_ready &= ~(1ul << 1);
			CPRINTS("eSPI FC ISR: Detected Master disabled FC Channel");
			TRACE0(64, ESPI, 0, "eSPI FC Disable");
		}
	} else {
		/* TODO handle FC command status: done and/or errors */
		CPRINTS("eSPI FC status = 0x%x", sts);
		TRACE11(65, ESPI, 0, "eSPI FC Status = 0x%08x",sts);
	}
}
DECLARE_IRQ(MEC17XX_IRQ_ESPI_FC, espi_fc_isr, 2);


/* eSPI Peripheral Channel interrupt handler */
void espi_pc_isr(void)
{
	uint32_t sts;

	sts = MEC17XX_ESPI_PC_STATUS;
	MEC17XX_ESPI_PC_STATUS = sts;
	MEC17XX_INT_SOURCE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_PC_GIRQ_BIT;
	if (sts & (1ul << 25)) {
		if (sts & (1ul << 24)) {
			MEC17XX_ESPI_IO_PC_READY = 1;
			espi_channels_ready |= (1ul << 3);
			CPRINTS("eSPI PC Channel Enable");
			TRACE0(66, ESPI, 0, "eSPI PC Enable");
		} else {
			espi_channels_ready &= ~(1ul << 3);
			CPRINTS("eSPI PC Channel Disable");
			TRACE0(67, ESPI, 0, "eSPI PC Disable");
		}

	} else {
		/* TODO */
		CPRINTS("eSPI PC status = 0x%x", sts);
		TRACE11(68, ESPI, 0, "eSPI PC Status = 0x%08x",sts);
	}
}
DECLARE_IRQ(MEC17XX_IRQ_ESPI_PC, espi_pc_isr, 2);


/*****************************************************************************/

/*
 * Enable/disable direct mode interrupt for ESPI_RESET# change.
 * Optionally clear status before enable or after disable.
 */
static void espi_reset_ictrl(int enable, int clr_status)
{
	if (enable) {
		if (clr_status) {
			MEC17XX_ESPI_IO_RESET_STATUS = MEC17XX_ESPI_RST_CHG_STS;
			MEC17XX_INT_SOURCE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_RESET_GIRQ_BIT;
		}
		MEC17XX_ESPI_IO_RESET_IEN |= MEC17XX_ESPI_RST_IEN;
		MEC17XX_INT_ENABLE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_RESET_GIRQ_BIT;
		task_enable_irq(MEC17XX_IRQ_ESPI_RESET);
	} else {
		task_disable_irq(MEC17XX_IRQ_ESPI_RESET);
		MEC17XX_INT_DISABLE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_RESET_GIRQ_BIT;
		MEC17XX_ESPI_IO_RESET_IEN &= ~(MEC17XX_ESPI_RST_IEN);
		if (clr_status) {
			MEC17XX_ESPI_IO_RESET_STATUS = MEC17XX_ESPI_RST_CHG_STS;
			MEC17XX_INT_SOURCE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_RESET_GIRQ_BIT;
		}
	}
}

/*
 * Enable/disable direct mode interrupts for eSPI VWire channel enable change.
 * Optionally clear status before enable or after disable.
 */
#if 0 /* The compiler is set to generate errors on unused functions! */
static void espi_vw_ictrl(int enable, int clr_status)
{
	if (enable) {
		if (clr_status) {
			MEC17XX_INT_SOURCE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_VW_EN_GIRQ_BIT;
		}
		MEC17XX_INT_ENABLE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_VW_EN_GIRQ_BIT;
		task_enable_irq(MEC17XX_IRQ_ESPI_VW_EN);
	} else {
		task_disable_irq(MEC17XX_IRQ_ESPI_VW_EN);
		MEC17XX_INT_DISABLE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_VW_EN_GIRQ_BIT;
		if (clr_status) {
			MEC17XX_INT_SOURCE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_VW_EN_GIRQ_BIT;
		}
	}
}
#endif

/*
 * TODO - eSPI channel interrupt control is becoming too complicated due to
 * the following.
 * 1. Three layers of enable and status bits.
 * 	channel enable and status
 * 	GIRQ enable and status
 * 	NVIC enable.
 * 	NVIC status is automatically cleared if the NVIC input
 * 	is cleared before the ISR exits.
 *
 * 2. Firmware may or may not need to clear interrupt status before enabling.
 *    Firmware may or may not need to clear interrupt status after disabling.
 *
 * 3. Some eSPI channels have Done and Enable-Change interrupt sources.
 *
 * 4. Firmware will enable/disable individual channel sources independently
 *    of each other.
 */
/*
 * Enable/disable direct mode interrupts for eSPI Flash channel enable change
 * and/or flash channel transfer done.
 * Optionally clear status before enable or after disable.
 * mask value b[0]=Done, b[1]=EnableChange
 */
#if 0
static void espi_fc_ictrl(int int_id, int enable, int clr)
{
	uint32_t bitpos = 0;
	uint32_t clr_mask = (1ul << 2); /* bit[2] */
	if (int_id) {
		bitpos++;
		clr_mask >>= 1; /* bit[1] */
	}

	if (enable) {
		if (clr) {
			MEC17XX_ESPI_FC_STATUS = clr_mask;
		}

		MEC17XX_ESPI_FC_IEN |= (1 << bitpos);
		MEC17XX_INT_ENABLE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_FC_GIRQ_BIT;
		task_enable_irq(MEC17XX_IRQ_ESPI_FC);
	} else {
		MEC17XX_ESPI_FC_IEN &= ~(1 << bitpos);
		if (0 == MEC17XX_ESPI_FC_IEN) {
			MEC17XX_INT_DISABLE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_FC_GIRQ_BIT;
			task_disable_irq(MEC17XX_IRQ_ESPI_FC);
		}

		if (clr) {
			MEC17XX_ESPI_FC_STATUS = clr_mask;
		}
	}
}
#endif

/*
 * Enable/disable direct mode interrupts for eSPI Flash OOB_TX/RX transfers
 * and/or OOB Enable change.
 * Optionally clear status before enable or after disable.
 * Note, two status registers, OOB_TX_STATUS and OOB_RX_STATUS. Channel
 * enable change is in OOB_TX.
 * OOB_RX Interrupt enable has bit[0] = RX Done
 * OOB_RX Status b[0]=RX Done, other bits do not generate interrupt
 * OOB_TX Interrupt enable has bits[1:0], b[0]=TX Done, b[1]=Enable-Change
 * OOB_TX Status b[0]=TX Done, b[1]=Enable-Change, other bits do not generate
 * interrupt.
 *
 * int_id = 0 TX Done
 *          1 EnableChange
 *          2 RX Done
 */
#if 0
static void espi_oob_rx_ictrl(int enable, int clr)
{
	uint8_t mask;

	mask = (uint8_t)(enable) & 0x01;

	task_disable_irq(MEC17XX_IRQ_ESPI_OOB_DN);

	MEC17XX_INT_DISABLE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_OOB_RX_GIRQ_BIT;
	MEC17XX_ESPI_OOB_RX_IEN &= ~(mask);
	MEC17XX_ESPI_OOB_RX_STATUS = MEC17XX_ESPI_OOB_RX_STATUS;
	MEC17XX_INT_SOURCE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_OOB_RX_GIRQ_BIT;

	if (enable) {
		MEC17XX_ESPI_OOB_RX_IEN = mask;
		MEC17XX_INT_ENABLE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_OOB_RX_GIRQ_BIT;
		task_enable_irq(MEC17XX_IRQ_ESPI_OOB_DN);
	}
}
#endif

/*
 * Enable/disable direct mode interrupts for eSPI Flash OOB_TX/RX transfers
 * and/or OOB Enable change.
 * Optionally clear status before enable or after disable.
 * Note, two status registers, OOB_TX_STATUS and OOB_RX_STATUS. Channel
 * enable change is in OOB_TX.
 * OOB_RX Interrupt enable has bit[0] = RX Done
 * OOB_RX Status b[0]=RX Done, other bits do not generate interrupt
 * OOB_TX Interrupt enable has bits[1:0], b[0]=TX Done, b[1]=Enable-Change
 * OOB_TX Status b[0]=TX Done, b[1]=Enable-Change, other bits do not generate
 * interrupt.
 *
 * int_id = 0 TX Done
 *          1 EnableChange
 *          2 RX Done
 */
#if 0
static void espi_oob_tx_ictrl(int enable)
{
	uint8_t mask;

	mask = (uint8_t)(enable) & 0x03;

	task_disable_irq(MEC17XX_IRQ_ESPI_OOB_UP);

	MEC17XX_INT_DISABLE(MEC17XX_ESPI_GIRQ) = (1ul << 6);
	MEC17XX_ESPI_OOB_TX_IEN &= ~(mask);
	MEC17XX_ESPI_OOB_TX_STATUS = MEC17XX_ESPI_OOB_RX_STATUS;
	MEC17XX_INT_SOURCE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_OOB_TX_GIRQ_BIT;

	if (enable) {
		MEC17XX_ESPI_OOB_TX_IEN = mask;
		MEC17XX_INT_ENABLE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_OOB_TX_GIRQ_BIT;
		task_enable_irq(MEC17XX_IRQ_ESPI_OOB_UP);
	}
}
#endif

#if 0
static void espi_pc_ictrl(int enable)
{
	uint32_t mask;

	mask = (uint32_t)(enable) & 0x12030003ul;

	task_disable_irq(MEC17XX_IRQ_ESPI_PC);

	MEC17XX_INT_DISABLE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_PC_GIRQ_BIT;
	MEC17XX_ESPI_PC_IEN &= ~(mask);
	MEC17XX_ESPI_PC_STATUS = MEC17XX_ESPI_PC_STATUS;
	MEC17XX_INT_SOURCE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_PC_GIRQ_BIT;

	if (enable) {
		MEC17XX_ESPI_PC_IEN = mask;
		MEC17XX_INT_ENABLE(MEC17XX_ESPI_GIRQ) = MEC17XX_ESPI_PC_GIRQ_BIT;
		task_enable_irq(MEC17XX_IRQ_ESPI_PC);
	}
}
#endif


/*
 * Is eSPI Virtual Wire channel ready?
 * 0 = Not ready
 * Non-zero ready
 * Return non-zero only if both VW and FC are ready
 */
int espi_chan_ready(void)
{
	int ready;

	interrupt_disable();
	ready = espi_channels_ready;
	interrupt_enable();

	if (0x03 == (ready & 0x03)) {
		return 1;
	}
	return 0;
}

/* eSPI Initialization functions */

/* MEC1701H */
void espi_init(void)
{
	espi_channels_ready = 0;

	CPRINTS("eSPI - espi_init");
	TRACE0(69, ESPI, 0, "eSPI Init");

	/*
	 * There is no MODULE_ESPI in include/module_id.h
	 * eSPI pins marked as MODULE_LPC in board/myboard/board.h
	 * eSPI pins are on VTR3.
	 * Make sure VTR3 chip knows VTR3 is 1.8V
	 * This is done in system_pre_init()
	 */
	gpio_config_module(MODULE_LPC, 1);

	MEC17XX_PCR_SLP_EN2 &= ~(1ul << 19);

#ifdef CONFIG_ESPI_CAP
	/* Override Boot-ROM configuration */
	/*
	 * #define CONFIG_ESPI_CAP0_VAL 0x0F b[3:0]=support FC:OOB:VW:PC
	 * #define CONFIG_ESPI_CAP1_VAL 0x00 b[2:0]=000b(20MHz) b[5:4]=00b(1X)
	 */
	MEC17XX_ESPI_IO_CAP0 = CONFIG_ESPI_CAP0_VAL;
	MEC17XX_ESPI_IO_CAP1 = CONFIG_ESPI_CAP1_VAL;
#endif

#ifdef CONFIG_ESPI_PLTRST_PIN
	MEC17XX_ESPI_IO_PLTRST_SRC = MEC17XX_ESPI_PLTRST_SRC_PIN;
#else
	MEC17XX_ESPI_IO_PLTRST_SRC = MEC17XX_ESPI_PLTRST_SRC_VW;
#endif

	MEC17XX_PCR_PWR_RST_CTL &= ~(1ul << MEC17XX_PCR_PWR_HOST_RST_SEL_BITPOS);

	MEC17XX_ESPI_ACTIVATE = 1;

	espi_bar_pre_init();

	/* TODO any VWire configuration?
	 * VWires are configured to be reset by different events.
	 * Default configuration has:
	 * RESET_SYS (chip reset) MSVW00, MSVW04
	 * RESET_ESPI MSVW01, MSVW03, SMVW00, SMVW01
	 * PLTRST MSVW02, SMVW02
	 */
	espi_vw_pre_init();

	/* Configure MSVW00 & MSVW04
	 * Any change to default values (SRCn bits)
	 * Any change to interrupt enable, SRCn_IRQ_SELECT bit fields
	 *   Should interrupt bits in MSVWyx and GIRQ24/25 be touched
	 *   before ESPI_RESET# de-asserts?
	 */

	MEC17XX_ESPI_PC_STATUS = 0xfffffffful;
	MEC17XX_ESPI_OOB_RX_STATUS = 0xfffffffful;
	MEC17XX_ESPI_FC_STATUS = 0xfffffffful;
	MEC17XX_INT_DISABLE(MEC17XX_ESPI_GIRQ) = 0x1FFul;
	MEC17XX_INT_SOURCE(MEC17XX_ESPI_GIRQ) = 0x1FFul;

	task_enable_irq(MEC17XX_IRQ_ESPI_PC);
	task_enable_irq(MEC17XX_IRQ_ESPI_OOB_UP);
	task_enable_irq(MEC17XX_IRQ_ESPI_OOB_DN);
	task_enable_irq(MEC17XX_IRQ_ESPI_FC);
	task_enable_irq(MEC17XX_IRQ_ESPI_VW_EN);

	/* Enable eSPI Master-to-Slave Virtual wire NVIC inputs
	 * VWire block interrupts are all disabled by default
	 * and will be controlled by espi_vw_enable/disable_wire_in
	 */
	CPRINTS("eSPI - enable ESPI_RESET# interrupt");
	TRACE0(70, ESPI, 0, "Enable ESPI_RESET# interrupt");

	/* Enable ESPI_RESET# interrupt and clear status */
	espi_reset_ictrl(1, 1);

	CPRINTS("eSPI - espi_init - done");
	TRACE0(71, ESPI, 0, "eSPI Init Done");

}



static int command_espi(int argc, char **argv)
{
	uint32_t chan;
	char *e;
	volatile struct mec17xx_espi_msvw *msvw;
	volatile struct mec17xx_espi_smvw *smvw;

	if (argc == 1) {
		return EC_ERROR_INVAL;
	/* Get value of eSPI registers */
	} else if (argc == 2) {
		int i;

		if (strcasecmp(argv[1], "cfg") == 0) {
			ccprintf("eSPI Reg32A [0x%08x]\n", MEC17XX_ESPI_IO_REG32_A);
			ccprintf("eSPI Reg32B [0x%08x]\n", MEC17XX_ESPI_IO_REG32_B);
			ccprintf("eSPI Reg32C [0x%08x]\n", MEC17XX_ESPI_IO_REG32_C);
			ccprintf("eSPI Reg32D [0x%08x]\n", MEC17XX_ESPI_IO_REG32_D);
		} else if (strcasecmp(argv[1], "vsm") == 0) {
			msvw = (volatile struct mec17xx_espi_msvw *)(MEC17XX_ESPI_MSVW_BASE);
			for (i = 0; i < MSVW_MAX; i++) {

				ccprintf("MSVW%d: 0x%08x:%08x:%08x\n", i, msvw[i].w2,
						msvw[i].w1, msvw[i].w0);
			}
		} else if (strcasecmp(argv[1], "vms") == 0) {
			smvw = (volatile struct mec17xx_espi_smvw *)(MEC17XX_ESPI_SMVW_BASE);
			for (i = 0; i < SMVW_MAX; i++) {

				ccprintf("SMVW%d: 0x%08x:%08x\n", i, smvw[i].w1,
						smvw[i].w0);
			}
		}
	/* Enable/Disable the channels of eSPI */
	} else if (argc == 3) {
		uint32_t m = (uint32_t) strtoi(argv[2], &e, 0);

		if (*e)
			return EC_ERROR_PARAM2;
		if (m < 0 || m > 4)
			return EC_ERROR_PARAM2;
		else if (m == 4)
			chan = 0x0F;
		else
			chan = 0x01 << m;
		if (strcasecmp(argv[1], "en") == 0)
			MEC17XX_ESPI_IO_CAP0 |= chan;
		else if (strcasecmp(argv[1], "dis") == 0)
			MEC17XX_ESPI_IO_CAP0 &= ~chan;
		else
			return EC_ERROR_PARAM1;
		ccprintf("eSPI IO Cap0 [0x%02x]\n", MEC17XX_ESPI_IO_CAP0);
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(espi, command_espi,
			"cfg/vms/vsm/en/dis [channel]",
			"eSPI configurations");


/* #endif  #ifdef CONFIG_ESPI */
