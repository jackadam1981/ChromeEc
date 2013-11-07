/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ITE83xx SoC In-System Debugging tool
 */

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "itecommon.h"

#if 0
	"SMFI"  0x1000
	"INTC"  0x1100
	"EC2I"  0x1200
	"KBC"   0x1300
	"SWUC"  0x1400
	"PMC"   0x1500
	"GPIO"  0x1600
	"PS2"   0x1700
	"PWM"   0x1800
	"ADC"   0x1900
	"DAC"   0x1A00
	"WUC"   0x1B00
	"SMB"   0x1C00
	"KBS"   0x1D00
	"ECPM"  0x1E00
	"ETWD"  0x1F00
	"GCTRL" 0x2000
	"EGPIO" 0x2100
	"BRAM"  0x2200
	"CIR"   0x2300
	"DBGR"  0x2500
	"SSPI"  0x2600
	"UART0" 0x2700
        "UART1" 0x2800
	"TMR"   0x2900
	"OW"    0x2A00
	"PECI"  0x2C00
	"I2C"   0x2D00
	"CEC"   0x2E00
	"USB"   0x2F00


	"DLL"         0x00
	"DLM"         0x01
	"RBR"         0x00
	"THR"         0x00
	"IER"         0x01
	"IIR"         0x02
	"FCR"         0x02
	"LCR"         0x03
	"MCR"         0x04
	"LSR"         0x05
	"MSR"         0x06
	"SCR"         0x07
	"ECSMPR"      0x08
	"CSSR"        0x09
#endif

#define JTAG_BRIDGE_BASE   0x2DC0
#define JTAG_BRIDGE_CR    (JTAG_BRIDGE_BASE + 0x00)
#define JTAG_BRIDGE_IR    (JTAG_BRIDGE_BASE + 0x01)
#define JTAG_BRIDGE_TDR   (JTAG_BRIDGE_BASE + 0x03)
#define JTAG_BRIDGE_RDR   (JTAG_BRIDGE_BASE + 0x04)
#define JTAG_BRIDGE_DIMIR (JTAG_BRIDGE_BASE + 0x06)

/* Instruction register codes */
enum {
	IR_BYPASS = 0xF,
	IR_IDCODE = 0x9,
	IR_EXECUTE = 0x8,
	IR_GET_DBG_EVENT = 0x7,
	IR_FAST_MEM_ACCESS = 0x6,
	IR_ACCESS_MISC_REG = 0x5,
	IR_ACCESS_MEM_B = 0xB,
	IR_ACCESS_MEM_H = 0xA,
	IR_ACCESS_MEM_W = 0x4,
	IR_ACCESS_DTR = 0x3,
	IR_ACCESS_DTM_SR = 0x2,
	IR_ACCESS_DIM = 0x1,
	IR_BSCAN = 0xE,
};

/* EDM System Registers indexes */
enum {
	SYS_REG_BPC0     = 0x00,
	SYS_REG_BPA0     = 0x08,
	SYS_REG_BPAM0    = 0x10,
	SYS_REG_BPAV0    = 0x18,
	SYS_REG_BPCID0   = 0x20,
	SYS_REG_EDM_CFG  = 0x28,
	SYS_REG_EDM_SW   = 0x30,
	SYS_REG_EDM_CTL  = 0x38,
	SYS_REG_EDM_DTR  = 0x40,
	SYS_REG_BPMTC    = 0x48,
	SYS_REG_DIMBR    = 0x50,
	SYS_REG_TECR0    = 0x70,
	SYS_REG_TECR1    = 0x71,
};

/* EDM Misc. registers indexes */
enum {
	MISC_REG_DIMIR     = 0x0,
	MISC_REG_SBAR      = 0x1,
	MISC_REG_EDM_CMDR  = 0x2,
	MISC_REG_DBGER     = 0x3,
	MISC_REG_ACC_CTL   = 0x4,
	MISC_REG_EDM_PROB  = 0x5,
	MISC_REG_GEN_PORT0 = 0x6,
	MISC_REG_GEN_PORT1 = 0x7,
};

/* Debug events */
enum {
	DBG_EVENT_DEX  = (1 << 0),
	DBG_EVENT_DPED = (2 << 0),
	DBG_EVENT_CRST = (3 << 0),
};

/* Use Andestar v3m opcodes */
#define NOP			0x40000009
#define DSB			0x64000008
#define ISB			0x64000009
#define BEQ_MINUS_12		0x4C003FFA
#define MTSR(reg,sr)		(0x64000003 | ((reg) << 20) | ((sr) << 10))
#define MFSR(reg,sr)		(0x64000002 | ((reg) << 20) | ((sr) << 10))

/* System registers */
#define SR_IDX(major, minor, ext)	(((major) << 7) | ((minor) << 3) | (ext))

#define SR_CPU_VER		SR_IDX(0, 0, 0)
#define SR_CORE_ID		SR_IDX(0, 0, 1)
#define SR_PSW			SR_IDX(1, 0, 0)
#define SR_IPC			SR_IDX(1, 5, 1)
#define SR_SP_USR		SR_IDX(1,10, 0)
#define SR_SP_PRIV		SR_IDX(1,10, 1)
#define SR_DTR			SR_IDX(3, 8, 0)

/* JTAG initialized ? */
int jtag_initialized = 0;

/* last JTAG IR value */
int jtag_ir = -1;

static uint8_t d2ec_read8(void *ctxt, uint16_t addr)
{
	int ret = 0;
	uint8_t data;

	ret |= i2c_write_byte(ctxt, 0x2F, addr >> 8);
	ret |= i2c_write_byte(ctxt, 0x2E, addr & 0xff);
	ret |= i2c_read_byte(ctxt,  0x30, &data);
	if (ret) {
		fprintf(stderr,"FAIL read8 at 0x%04x\n", addr);
		printf("FAIL read8 at 0x%04x\n", addr);
		data = 0xEE;
	}
	return data;
}

static int d2ec_write8(void *ctxt, uint16_t addr, uint8_t val)
{
	int ret = 0;

	ret |= i2c_write_byte(ctxt, 0x2F, addr >> 8);
	ret |= i2c_write_byte(ctxt, 0x2E, addr & 0xff);
	ret |= i2c_write_byte(ctxt,  0x30, val);
	if (ret)
		fprintf(stderr,"FAIL write8 at 0x%04x\n", addr);
	return 0;
}

static void d2ec(void *ctxt)
{
	int ret;

	/* Enable D2EC mode */
	ret = i2c_write_byte(ctxt, 0x27, 0x00);
	if (ret)
		fprintf(stderr,"FAIL\n");

	printf("READ %02x%02x %02x\n", d2ec_read8(ctxt, 0x2000), d2ec_read8(ctxt, 0x2001), d2ec_read8(ctxt, 0x2002));
}


static void jtag_init(void *ctxt)
{
	int ret;

	/* pulse TRST */
	d2ec_write8(ctxt, JTAG_BRIDGE_CR, 0x80);
	/* TRST de-asserted, no interrupt */
	ret = d2ec_write8(ctxt, JTAG_BRIDGE_CR, 0x81);
	if (!ret)
		jtag_initialized = 1;
}

static void jtag_set_ir(void *ctxt, int ir)
{
	int ret;

	if (!jtag_initialized)
		jtag_init(ctxt);

	ret = d2ec_write8(ctxt, JTAG_BRIDGE_IR, ir);
	if (!ret)
		jtag_ir = ir;
}

static uint64_t jtag_shift(void *ctxt, int ir, int nbits, uint64_t data)
{
	uint8_t *buf = (uint8_t *)&data;
	int bytes = (nbits + 7) / 8;
	int i;

	if (ir != jtag_ir)
		jtag_set_ir(ctxt, ir);

	for (i = 0; i < bytes; i++) {
		d2ec_write8(ctxt, JTAG_BRIDGE_TDR, buf[i]);
		buf[i] = d2ec_read8(ctxt, JTAG_BRIDGE_RDR);
	}
	/* shift properly toward LSB the last unterminated byte */
	buf[i - 1] = buf[i - 1] >> (bytes * 8 - nbits);

	return data;
}

static uint32_t jtag_idcode(void *ctxt)
{
	uint32_t idcode;
	uint16_t part;
	uint16_t manuf;

	idcode = jtag_shift(ctxt, IR_IDCODE, 32, 0xdeadbeef);
	part = (idcode >> 12) & 0xFFFF;
	manuf = (idcode >> 1) & 0x7FF;
	printf("IDCODE %04x JDP ver %d Part Number %04x Manuf ID %03x (%s)\n",
		idcode, idcode >> 28, part, manuf,
		manuf == 0x31e ? "Andes" : "Unknown");

	return idcode;
}

static uint32_t jtag_access_x(void *ctxt, int ir, int abits, int dbits,
			      uint32_t addr, uint32_t data, int write)
{
	uint64_t ibuf, obuf;
	/* control bits (rW, InC) position */
	int cshift = abits + dbits;
	const uint64_t in_c = 2ULL << cshift;
	//printf("abits %d dbits %d cshift %d\n", abits,dbits,cshift);

	ibuf = 0;
	if (write)
		ibuf |= data;
	ibuf |= (uint64_t)addr << dbits;
	ibuf |= in_c | (write ? (1ULL << cshift) : 0);
	//printf(">>>> %016lx", ibuf);
	if (!write) {
		obuf = jtag_shift(ctxt, ir, 2 + cshift, ibuf);
		//printf("=== %016lx", obuf);
		ibuf &= ~in_c;
	}
	do {
		obuf = jtag_shift(ctxt, ir, 2 + cshift, ibuf);
		ibuf &= ~in_c;
		//printf(">>> %016lx\n", obuf);
		/* retry if InC is not set */
	} while (!(obuf & in_c));

	return obuf;
}

uint8_t jtag_read_mem_b(void *ctxt, uint32_t addr)
{
	return jtag_access_x(ctxt, IR_ACCESS_MEM_B, 32, 8, addr,
			     0xdeadbeef, 0);
}
void jtag_write_mem_b(void *ctxt, uint32_t addr, uint8_t val)
{
	jtag_access_x(ctxt, IR_ACCESS_MEM_B, 32, 8, addr, val, 1);
}
uint16_t jtag_read_mem_h(void *ctxt, uint32_t addr)
{
	return jtag_access_x(ctxt, IR_ACCESS_MEM_H, 31, 16, addr >> 1,
			     0xdeadbeef, 0);
}
void jtag_write_mem_h(void *ctxt, uint32_t addr, uint16_t val)
{
	jtag_access_x(ctxt, IR_ACCESS_MEM_H, 31, 16, addr >> 1, val, 1);
}
uint32_t jtag_read_mem_w(void *ctxt, uint32_t addr)
{
	return jtag_access_x(ctxt, IR_ACCESS_MEM_W, 30, 32, addr >> 2,
			     0xdeadbeef, 0);
}
void jtag_write_mem_w(void *ctxt, uint32_t addr, uint32_t val)
{
	jtag_access_x(ctxt, IR_ACCESS_MEM_W, 30, 32, addr >> 2, val, 1);
}

uint32_t jtag_read_dim(void *ctxt)
{
	return jtag_access_x(ctxt, IR_ACCESS_DIM, 0, 32, 0xdeadbeef,
			     0xdeadbeef, 0);
}
void jtag_write_dim(void *ctxt, uint32_t instr)
{
	jtag_access_x(ctxt, IR_ACCESS_DIM, 0, 32, 0xdeadbeef, instr, 1);
}
uint32_t jtag_read_dtr(void *ctxt)
{
	return jtag_access_x(ctxt, IR_ACCESS_DTR, 0, 32, 0xdeadbeef,
			     0xdeadbeef, 0);
}
void jtag_write_dtr(void *ctxt, uint32_t instr)
{
	jtag_access_x(ctxt, IR_ACCESS_DTR, 0, 32, 0xdeadbeef, instr, 1);
}

uint32_t jtag_read_misc_reg(void *ctxt, int idx)
{
	return jtag_access_x(ctxt, IR_ACCESS_MISC_REG, 4, 32, idx & 0xF,
			     0xdeadbeef, 0);
}
void jtag_write_misc_reg(void *ctxt, int idx, uint32_t val)
{
	jtag_access_x(ctxt, IR_ACCESS_MISC_REG, 4, 32, idx & 0xF, val, 1);
}
uint32_t jtag_read_sys_reg(void *ctxt, int idx)
{
	return jtag_access_x(ctxt, IR_ACCESS_DTM_SR, 7, 32, idx & 0x7F,
			     0xdeadbeef, 0);
}
void jtag_write_sys_reg(void *ctxt, int idx, uint32_t val)
{
	jtag_access_x(ctxt, IR_ACCESS_DTM_SR, 7, 32, idx & 0x7F, val, 1);
}

int jtag_get_dbg_event(void *ctxt)
{
	return jtag_shift(ctxt, IR_GET_DBG_EVENT, 4, 0xFF) & 0xF;
}

static uint32_t last_sbar = 0xffffffff;
uint32_t jtag_read_fast_mem(void *ctxt, uint32_t addr)
{
	uint64_t buf = 1ULL << 32;
	uint32_t sbar = addr & 0xFFFFFFFC;

	if (sbar != last_sbar) {
		jtag_write_misc_reg(ctxt, MISC_REG_SBAR,sbar);
		last_sbar = sbar;
	}
	buf = jtag_shift(ctxt, IR_FAST_MEM_ACCESS, 33, buf);
	/* TODO: manage InC flag ? */

	last_sbar += 4;
	return buf & 0xFFFFFFFF;
}
void jtag_write_fast_mem(void *ctxt, uint32_t addr, uint32_t val)
{
	uint64_t buf = val | (1ULL << 32);
	uint32_t sbar = (addr & 0xFFFFFFFC) | 1;

	if (sbar != last_sbar) {
		jtag_write_misc_reg(ctxt, MISC_REG_SBAR,sbar);
		last_sbar = sbar;
	}

	jtag_shift(ctxt, IR_FAST_MEM_ACCESS, 33, buf);
	/* TODO: manage InC flag ? */

	last_sbar += 4;
}


void jtag_execute(void *ctxt, uint32_t *instrs, int count)
{
	int i;

	/* load instructions into DIM */
	for (i = 0; i < count; i++)
		jtag_write_dim(ctxt, instrs[i]);

	/* clear DPED flag */
	jtag_write_misc_reg(ctxt, MISC_REG_DBGER, DBG_EVENT_DPED);

	/* Fetch DIM content */
	jtag_set_ir(ctxt, IR_EXECUTE);

	/* Check the "Debug Program Execution Done" flag */
	if (!(jtag_get_dbg_event(ctxt) & DBG_EVENT_DPED))
		fprintf(stderr, "execution not completed !\n");

	/* Go back to normal */
	jtag_set_ir(ctxt, IR_BYPASS);
}

void jtag_set_reg(void *ctxt, int idx, uint32_t val)
{
	uint32_t edm_sw;
	uint32_t instrs[] = {MFSR(idx, SR_DTR), DSB, NOP, BEQ_MINUS_12};

	/* store the value to write in DTRegister */
	jtag_write_sys_reg(ctxt, SYS_REG_EDM_DTR, val);

	edm_sw = jtag_read_sys_reg(ctxt, SYS_REG_EDM_SW);
	if (!(edm_sw & 1))
		fprintf(stderr, "Failed to write DTL\n");

	/* execute sequence using DIMU */
	jtag_execute(ctxt, instrs, 4);
}
uint32_t jtag_get_reg(void *ctxt, int idx)
{
	uint32_t edm_sw;
	uint32_t instrs[] = {MTSR(idx, SR_DTR), DSB, NOP, BEQ_MINUS_12};

	/* execute sequence using DIMU */
	jtag_execute(ctxt, instrs, 4);

	edm_sw = jtag_read_sys_reg(ctxt, SYS_REG_EDM_SW);
	if (!(edm_sw & 2))
		fprintf(stderr, "Cannot read DTL\n");
	/* get the value stored in DTRegister */
	return jtag_read_sys_reg(ctxt, SYS_REG_EDM_DTR);
}

void jtag_set_sr(void *ctxt, int sridx, uint32_t val)
{
	uint32_t edm_sw;
	uint32_t instrs[] = {MFSR(15, SR_DTR), MTSR(15, sridx),
			     DSB, BEQ_MINUS_12};

	/* store the value to write in DTRegister */
	jtag_write_sys_reg(ctxt, SYS_REG_EDM_DTR, val);

	edm_sw = jtag_read_sys_reg(ctxt, SYS_REG_EDM_SW);
	if (!(edm_sw & 1))
		fprintf(stderr, "Failed to write DTR\n");

	/* execute sequence using DIMU */
	jtag_execute(ctxt, instrs, 4);
}
uint32_t jtag_get_sr(void *ctxt, int sridx)
{
	uint32_t edm_sw;
	uint32_t instrs[] = {MTSR(15, SR_DTR), MFSR(15, sridx),
			     DSB, BEQ_MINUS_12};

	/* execute sequence using DIMU */
	jtag_execute(ctxt, instrs, 4);

	edm_sw = jtag_read_sys_reg(ctxt, SYS_REG_EDM_SW);
	if (!(edm_sw & 2))
		fprintf(stderr, "Cannot read DTR\n");
	/* get the value stored in DTL register */
	return jtag_read_sys_reg(ctxt, SYS_REG_EDM_DTR);
}


void jtag_edm_init(void *ctxt)
{
	uint32_t edm_ctl, edm_cfg;

	/* set Debug Instruction Memory Base Address */
	jtag_write_sys_reg(ctxt, SYS_REG_DIMBR, 0);
	//jtag_write_sys_reg(ctxt, SYS_REG_DIMBR, 64 * 1024 - 4096);
	/* Reset DIM index */
	jtag_write_misc_reg(ctxt, MISC_REG_DIMIR, 0);

	/* unconditionally try to turn on V3_EDM_MODE */
	edm_ctl = jtag_read_sys_reg(ctxt, SYS_REG_EDM_CTL);
	jtag_write_sys_reg(ctxt, SYS_REG_EDM_CTL, edm_ctl | 0x40);

	/* clear debug event */
	jtag_write_misc_reg(ctxt, MISC_REG_DBGER, DBG_EVENT_DPED | DBG_EVENT_CRST);

	/* get EDM version */
	edm_cfg = jtag_read_sys_reg(ctxt, SYS_REG_EDM_CFG);
	printf("EDM V%d: CFG %08x\n", ((edm_cfg >> 28) & 0xF) + 2, edm_cfg);
}

static void jtag(void *ctxt)
{
	jtag_idcode(ctxt);

	jtag_edm_init(ctxt);

	printf("READ MEMB : %02x %02x %02x\n", jtag_read_mem_b(ctxt, 0xF02000), jtag_read_mem_b(ctxt, 0xF02001), jtag_read_mem_b(ctxt, 0xF02002));
	printf("READ MEMB : %02x %02x %02x\n", jtag_read_mem_b(ctxt, 0xF02000), jtag_read_mem_b(ctxt, 0xF02001), jtag_read_mem_b(ctxt, 0xF02002));
	printf("READ MEMB : %02x %02x %02x\n", jtag_read_mem_b(ctxt, 0xF02000), jtag_read_mem_b(ctxt, 0xF02001), jtag_read_mem_b(ctxt, 0xF02002));
	printf("READ MEMB : %02x %02x %02x\n", jtag_read_mem_b(ctxt, 0xF02000), jtag_read_mem_b(ctxt, 0xF02001), jtag_read_mem_b(ctxt, 0xF02002));
	printf("SCR %02x\n", jtag_read_mem_b(ctxt, 0xF02707));
	jtag_write_mem_b(ctxt, 0xF02707,0x55);
	printf("SCR %02x\n", jtag_read_mem_b(ctxt, 0xF02707));
	jtag_write_mem_b(ctxt, 0xF02707,0xBA);
	printf("SCR %02x\n", jtag_read_mem_b(ctxt, 0xF02707));
	printf("SCR %02x\n", jtag_read_mem_b(ctxt, 0xF02707));
	printf("READ MEMH : %04x %04x\n", jtag_read_mem_h(ctxt, 0xF02000), jtag_read_mem_h(ctxt, 0xF02002));
	printf("READ MEMH : %04x %04x\n", jtag_read_mem_h(ctxt, 0xF02000), jtag_read_mem_h(ctxt, 0xF02002));
	printf("READ MEMW : %08x\n", jtag_read_mem_w(ctxt, 0xF02000));
	printf("READ MEMW : %08x\n", jtag_read_mem_w(ctxt, 0xF02000));
}

static const struct option longopts[] = {
	COMMON_CMD_LONGOPTS,
	{"console", 0, 0, 'c'},
	{NULL, 0, 0, 0}
};

void display_usage(char *program)
{
	fprintf(stderr, "Usage: %s [-d] [-v <VID>] [-p <PID>] [-i <1|2>] "
		"[-s <serial>] [-c] \"<debug cmds>\"\n",
		program);
	display_common_usage();
	fprintf(stderr, "--c[onsole] : start interactive command console\n");

	exit(2);
}

int parse_parameters(int argc, char **argv)
{
	int opt, idx;
	int flags = 0;

	while ((opt = getopt_long(argc, argv, COMMON_CMD_FLAGS"c",
				  longopts, &idx)) != -1) {
		if (!parse_common_arg(opt, argv[0]))
			switch (opt) {
			case 'c':
				flags |= 1;
				break;
			}
	}
	return flags;
}

int tool_main(void *ctxt, int flags)
{
	d2ec(ctxt);
	jtag(ctxt);

	return 0;
}
