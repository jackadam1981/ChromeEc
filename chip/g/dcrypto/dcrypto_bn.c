/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "console.h"
#include "dcrypto.h"
#include "internal.h"
#include "registers.h"
#include "trng.h"

/* Firmware blob for crypto accelerator */

/* AUTO-GENERATED.  DO NOT MODIFY. */
static const uint32_t IMEM_dcrypto[] = {
/* @0x0: function tag[1] { */
#define CF_tag_adr 0
0xf8000001,	/* sigini #1 */
/* } */
/* @0x1: function d0inv[14] { */
#define CF_d0inv_adr 1
0x4c000000,	/* xor r0, r0, r0 */
0x80000001,	/* movi r0.0l, #1 */
0x7c740000,	/* mov r29, r0 */
0x05100008,	/* loop #256 ( */
0x5807bc00,	/* mul128 r1, r28l, r29l */
0x588bbc00,	/* mul128 r2, r28u, r29l */
0x50044110,	/* add r1, r1, r2 << 128 */
0x590bbc00,	/* mul128 r2, r28l, r29u */
0x50044110,	/* add r1, r1, r2 << 128 */
0x40040100,	/* and r1, r1, r0 */
0x44743d00,	/* or r29, r29, r1 */
0x50000000,	/* add r0, r0, r0 */
/*		   ) */
0x5477bf00,	/* sub r29, r31, r29 */
0x0c000000,	/* ret */
/* } */
/* @0xf: function selcxSub[10] { */
#define CF_selcxSub_adr 15
0x97800100,	/* ldrfp r1 */
0x95800000,	/* lddmp r0 */
0x540c6300,	/* sub r3, r3, r3 */
0x0600c005,	/* loop *6 ( */
0x8c081800,	/* ld *2, *0++ */
0x7c8c0000,	/* ldr *3, *0 */
0x54906200,	/* subb r4, r2, r3 */
0x66084408,	/* selcx r2, r4, r2 */
0x7ca00300,	/* ldr *0++, *3 */
/*		   ) */
0x0c000000,	/* ret */
/* } */
/* @0x19: function computeRR[41] { */
#define CF_computeRR_adr 25
0x4c7fff00,	/* xor r31, r31, r31 */
0x84004000,	/* ldi r0, [#0] */
0x95800000,	/* lddmp r0 */
0x4c0c6300,	/* xor r3, r3, r3 */
0x800cffff,	/* movi r3.0l, #65535 */
0x40040398,	/* and r1, r3, r0 >> 192 */
0x480c6000,	/* not r3, r3 */
0x400c0300,	/* and r3, r3, r0 */
0x500c2301,	/* add r3, r3, r1 << 8 */
0x94800300,	/* ldlc r3 */
0x80040005,	/* movi r1.0l, #5 */
0x81040003,	/* movi r1.2l, #3 */
0x81840002,	/* movi r1.3l, #2 */
0x82040004,	/* movi r1.4l, #4 */
0x97800100,	/* ldrfp r1 */
0x4c0c6300,	/* xor r3, r3, r3 */
0x0600c001,	/* loop *6 ( */
0x7ca00200,	/* ldr *0++, *2 */
/*		   ) */
0x560c1f00,	/* subx r3, r31, r0 */
0x0800000f,	/* call &selcxSub */
0x06000010,	/* loop *0 ( */
0x97800100,	/* ldrfp r1 */
0x560c6300,	/* subx r3, r3, r3 */
0x0600c003,	/* loop *6 ( */
0x7c8c0000,	/* ldr *3, *0 */
0x52884200,	/* addcx r2, r2, r2 */
0x7ca00300,	/* ldr *0++, *3 */
/*		   ) */
0x0800000f,	/* call &selcxSub */
0x97800100,	/* ldrfp r1 */
0x95800000,	/* lddmp r0 */
0x560c6300,	/* subx r3, r3, r3 */
0x0600c003,	/* loop *6 ( */
0x8c081800,	/* ld *2, *0++ */
0x7c8c0800,	/* ldr *3, *0++ */
0x5e804300,	/* cmpbx r3, r2 */
/*		   ) */
0x0800000f,	/* call &selcxSub */
0xfc000000,	/* nop */
/*		   ) */
0x97800100,	/* ldrfp r1 */
0x0600c001,	/* loop *6 ( */
0x90680800,	/* st *0++, *2++ */
/*		   ) */
0x0c000000,	/* ret */
/* } */
/* @0x42: function dmXd0[9] { */
#define CF_dmXd0_adr 66
0x586f3e00,	/* mul128 r27, r30l, r25l */
0x59eb3e00,	/* mul128 r26, r30u, r25u */
0x58df3e00,	/* mul128 r23, r30u, r25l */
0x506efb10,	/* add r27, r27, r23 << 128 */
0x50eafa90,	/* addc r26, r26, r23 >> 128 */
0x595f3e00,	/* mul128 r23, r30l, r25u */
0x506efb10,	/* add r27, r27, r23 << 128 */
0x50eafa90,	/* addc r26, r26, r23 >> 128 */
0x0c000000,	/* ret */
/* } */
/* @0x4b: function dmXa[9] { */
#define CF_dmXa_adr 75
0x586c5e00,	/* mul128 r27, r30l, r2l */
0x59e85e00,	/* mul128 r26, r30u, r2u */
0x58dc5e00,	/* mul128 r23, r30u, r2l */
0x506efb10,	/* add r27, r27, r23 << 128 */
0x50eafa90,	/* addc r26, r26, r23 >> 128 */
0x595c5e00,	/* mul128 r23, r30l, r2u */
0x506efb10,	/* add r27, r27, r23 << 128 */
0x50eafa90,	/* addc r26, r26, r23 >> 128 */
0x0c000000,	/* ret */
/* } */
/* @0x54: function mma[46] { */
#define CF_mma_adr 84
0x8204001e,	/* movi r1.4l, #30 */
0x82840018,	/* movi r1.5l, #24 */
0x97800100,	/* ldrfp r1 */
0x8c101b00,	/* ld *4, *3++ */
0x0800004b,	/* call &dmXa */
0x7c940800,	/* ldr *5, *0++ */
0x507b1b00,	/* add r30, r27, r24 */
0x50f7fa00,	/* addc r29, r26, r31 */
0x7c640300,	/* mov r25, r3 */
0x08000042,	/* call &dmXd0 */
0x7c641b00,	/* mov r25, r27 */
0x7c701a00,	/* mov r28, r26 */
0x7c601e00,	/* mov r24, r30 */
0x8c101800,	/* ld *4, *0++ */
0x08000042,	/* call &dmXd0 */
0x506f1b00,	/* add r27, r27, r24 */
0x50f3fa00,	/* addc r28, r26, r31 */
0x0600e00e,	/* loop *7 ( */
0x8c101b00,	/* ld *4, *3++ */
0x0800004b,	/* call &dmXa */
0x7c940800,	/* ldr *5, *0++ */
0x506f1b00,	/* add r27, r27, r24 */
0x50ebfa00,	/* addc r26, r26, r31 */
0x5063bb00,	/* add r24, r27, r29 */
0x50f7fa00,	/* addc r29, r26, r31 */
0x8c101800,	/* ld *4, *0++ */
0x08000042,	/* call &dmXd0 */
0x506f1b00,	/* add r27, r27, r24 */
0x50ebfa00,	/* addc r26, r26, r31 */
0x52639b00,	/* addx r24, r27, r28 */
0x7ca80500,	/* ldr *2++, *5 */
0x52f3fa00,	/* addcx r28, r26, r31 */
/*		   ) */
0x52e39d00,	/* addcx r24, r29, r28 */
0x7ca80500,	/* ldr *2++, *5 */
0x95800000,	/* lddmp r0 */
0x97800100,	/* ldrfp r1 */
0x54739c00,	/* sub r28, r28, r28 */
0x0600c007,	/* loop *6 ( */
0x8c141800,	/* ld *5, *0++ */
0x7c900000,	/* ldr *4, *0 */
0x54f71e00,	/* subb r29, r30, r24 */
0x99600000,	/* strnd r24 */
0x7c800500,	/* ldr *0, *5 */
0x6663dd08,	/* selcx r24, r29, r30 */
0x7ca00500,	/* ldr *0++, *5 */
/*		   ) */
0x0c000000,	/* ret */
/* } */
/* @0x82: function setupPtrs[11] { */
#define CF_setupPtrs_adr 130
0x847c4000,	/* ldi r31, [#0] */
0x4c7fff00,	/* xor r31, r31, r31 */
0x95800000,	/* lddmp r0 */
0x94800000,	/* ldlc r0 */
0x7c041f00,	/* mov r1, r31 */
0x80040004,	/* movi r1.0l, #4 */
0x80840003,	/* movi r1.1l, #3 */
0x81040004,	/* movi r1.2l, #4 */
0x81840002,	/* movi r1.3l, #2 */
0x97800100,	/* ldrfp r1 */
0x0c000000,	/* ret */
/* } */
/* @0x8d: function mulx[19] { */
#define CF_mulx_adr 141
0x84004000,	/* ldi r0, [#0] */
0x08000082,	/* call &setupPtrs */
0x8c041100,	/* ld *1, *1 */
0x7c081f00,	/* mov r2, r31 */
0x0600c001,	/* loop *6 ( */
0x7ca80300,	/* ldr *2++, *3 */
/*		   ) */
0x97800100,	/* ldrfp r1 */
0x0600c004,	/* loop *6 ( */
0x8c0c1c00,	/* ld *3, *4++ */
0x95000000,	/* stdmp r0 */
0x08000054,	/* call &mma */
0x95800000,	/* lddmp r0 */
/*		   ) */
0x97800100,	/* ldrfp r1 */
0x95800000,	/* lddmp r0 */
0x0600c001,	/* loop *6 ( */
0x90740800,	/* st *0++, *5++ */
/*		   ) */
0x97800100,	/* ldrfp r1 */
0x95800000,	/* lddmp r0 */
0x0c000000,	/* ret */
/* } */
/* @0xa0: function mul1_exp[30] { */
#define CF_mul1_exp_adr 160
0x8c041100,	/* ld *1, *1 */
0x7c081f00,	/* mov r2, r31 */
0x0600c001,	/* loop *6 ( */
0x7ca80300,	/* ldr *2++, *3 */
/*		   ) */
0x97800100,	/* ldrfp r1 */
0x80080001,	/* movi r2.0l, #1 */
0x0600c003,	/* loop *6 ( */
0x95800000,	/* lddmp r0 */
0x08000054,	/* call &mma */
0x7c081f00,	/* mov r2, r31 */
/*		   ) */
0x97800100,	/* ldrfp r1 */
0x95800000,	/* lddmp r0 */
0x56084200,	/* subx r2, r2, r2 */
0x0600c003,	/* loop *6 ( */
0x8c041800,	/* ld *1, *0++ */
0x7c8c0800,	/* ldr *3, *0++ */
0x5e804300,	/* cmpbx r3, r2 */
/*		   ) */
0x97800100,	/* ldrfp r1 */
0x95800000,	/* lddmp r0 */
0x540c6300,	/* sub r3, r3, r3 */
0x0600c006,	/* loop *6 ( */
0x8c041800,	/* ld *1, *0++ */
0x7c8c0800,	/* ldr *3, *0++ */
0x548c6200,	/* subb r3, r2, r3 */
0x66084308,	/* selcx r2, r3, r2 */
0x90740300,	/* st *3, *5++ */
0xfc000000,	/* nop */
/*		   ) */
0x97800100,	/* ldrfp r1 */
0x95800000,	/* lddmp r0 */
0x0c000000,	/* ret */
/* } */
/* @0xbe: function mul1[4] { */
#define CF_mul1_adr 190
0x84004000,	/* ldi r0, [#0] */
0x08000082,	/* call &setupPtrs */
0x080000a0,	/* call &mul1_exp */
0x0c000000,	/* ret */
/* } */
/* @0xc2: function sqrx_exp[19] { */
#define CF_sqrx_exp_adr 194
0x84004020,	/* ldi r0, [#1] */
0x95800000,	/* lddmp r0 */
0x8c041100,	/* ld *1, *1 */
0x7c081f00,	/* mov r2, r31 */
0x0600c001,	/* loop *6 ( */
0x7ca80300,	/* ldr *2++, *3 */
/*		   ) */
0x97800100,	/* ldrfp r1 */
0x0600c004,	/* loop *6 ( */
0x8c0c1c00,	/* ld *3, *4++ */
0x95000000,	/* stdmp r0 */
0x08000054,	/* call &mma */
0x95800000,	/* lddmp r0 */
/*		   ) */
0x97800100,	/* ldrfp r1 */
0x95800000,	/* lddmp r0 */
0x0600c001,	/* loop *6 ( */
0x90740800,	/* st *0++, *5++ */
/*		   ) */
0x97800100,	/* ldrfp r1 */
0x95800000,	/* lddmp r0 */
0x0c000000,	/* ret */
/* } */
/* @0xd5: function mulx_exp[14] { */
#define CF_mulx_exp_adr 213
0x84004040,	/* ldi r0, [#2] */
0x95800000,	/* lddmp r0 */
0x8c041100,	/* ld *1, *1 */
0x7c081f00,	/* mov r2, r31 */
0x0600c001,	/* loop *6 ( */
0x7ca80300,	/* ldr *2++, *3 */
/*		   ) */
0x97800100,	/* ldrfp r1 */
0x0600c004,	/* loop *6 ( */
0x8c0c1c00,	/* ld *3, *4++ */
0x95000000,	/* stdmp r0 */
0x08000054,	/* call &mma */
0x95800000,	/* lddmp r0 */
/*		   ) */
0x97800100,	/* ldrfp r1 */
0x0c000000,	/* ret */
/* } */
/* @0xe3: function modexp[43] { */
#define CF_modexp_adr 227
0x0800008d,	/* call &mulx */
0x84004060,	/* ldi r0, [#3] */
0x95800000,	/* lddmp r0 */
0x54084200,	/* sub r2, r2, r2 */
0x0600c004,	/* loop *6 ( */
0xfc000000,	/* nop */
0x8c0c1800,	/* ld *3, *0++ */
0x54885f00,	/* subb r2, r31, r2 */
0x90740300,	/* st *3, *5++ */
/*		   ) */
0xfc000000,	/* nop */
0x7c081f00,	/* mov r2, r31 */
0x8008ffff,	/* movi r2.0l, #65535 */
0x400c0298,	/* and r3, r2, r0 >> 192 */
0x48084000,	/* not r2, r2 */
0x40080200,	/* and r2, r2, r0 */
0x50086201,	/* add r2, r2, r3 << 8 */
0x94800200,	/* ldlc r2 */
0x06000015,	/* loop *0 ( */
0x080000c2,	/* call &sqrx_exp */
0x080000d5,	/* call &mulx_exp */
0x84004060,	/* ldi r0, [#3] */
0x95800000,	/* lddmp r0 */
0x99080000,	/* strnd r2 */
0x54084200,	/* sub r2, r2, r2 */
0x0600c004,	/* loop *6 ( */
0x99080000,	/* strnd r2 */
0x8c0c1400,	/* ld *3, *4 */
0x50884200,	/* addc r2, r2, r2 */
0x90700300,	/* st *3, *4++ */
/*		   ) */
0x0600c008,	/* loop *6 ( */
0x99080000,	/* strnd r2 */
0x8c041500,	/* ld *1, *5 */
0x90540300,	/* st *3, *5 */
0x7c8c0800,	/* ldr *3, *0++ */
0x7c000200,	/* mov r0, r2 */
0x99080000,	/* strnd r2 */
0x64086008,	/* selc r2, r0, r3 */
0x90740300,	/* st *3, *5++ */
/*		   ) */
0xfc000000,	/* nop */
/*		   ) */
0x84004060,	/* ldi r0, [#3] */
0x95800000,	/* lddmp r0 */
0x080000a0,	/* call &mul1_exp */
0x0c000000,	/* ret */
/* } */
/* @0x10e: function modexp_blinded[76] { */
#define CF_modexp_blinded_adr 270
0x0800008d,	/* call &mulx */
0x84004060,	/* ldi r0, [#3] */
0x95800000,	/* lddmp r0 */
0x54084200,	/* sub r2, r2, r2 */
0x0600c004,	/* loop *6 ( */
0xfc000000,	/* nop */
0x8c0c1800,	/* ld *3, *0++ */
0x54885f00,	/* subb r2, r31, r2 */
0x90740300,	/* st *3, *5++ */
/*		   ) */
0xfc000000,	/* nop */
0x8c0c1900,	/* ld *3, *1++ */
0x8c0c1100,	/* ld *3, *1 */
0x521c5f90,	/* addx r7, r31, r2 >> 128 */
0x590c4200,	/* mul128 r3, r2l, r2u */
0x7c181f00,	/* mov r6, r31 */
0x0600c011,	/* loop *6 ( */
0x99080000,	/* strnd r2 */
0x8c0c1400,	/* ld *3, *4 */
0x58106200,	/* mul128 r4, r2l, r3l */
0x59946200,	/* mul128 r5, r2u, r3u */
0x58806200,	/* mul128 r0, r2u, r3l */
0x50100410,	/* add r4, r4, r0 << 128 */
0x50940590,	/* addc r5, r5, r0 >> 128 */
0x59006200,	/* mul128 r0, r2l, r3u */
0x50100410,	/* add r4, r4, r0 << 128 */
0x50940590,	/* addc r5, r5, r0 >> 128 */
0x5010c400,	/* add r4, r4, r6 */
0x5097e500,	/* addc r5, r5, r31 */
0x50088200,	/* add r2, r2, r4 */
0x509be500,	/* addc r6, r5, r31 */
0x5688e200,	/* subbx r2, r2, r7 */
0x90700300,	/* st *3, *4++ */
0x541ce700,	/* sub r7, r7, r7 */
/*		   ) */
0x7c080600,	/* mov r2, r6 */
0x5688e200,	/* subbx r2, r2, r7 */
0x90500300,	/* st *3, *4 */
0xfc000000,	/* nop */
0x84004060,	/* ldi r0, [#3] */
0x7c081f00,	/* mov r2, r31 */
0x8008ffff,	/* movi r2.0l, #65535 */
0x400c0298,	/* and r3, r2, r0 >> 192 */
0x48084000,	/* not r2, r2 */
0x40080200,	/* and r2, r2, r0 */
0x510c0301,	/* addi r3, r3, #1 */
0x50086201,	/* add r2, r2, r3 << 8 */
0x94800200,	/* ldlc r2 */
0x06000019,	/* loop *0 ( */
0x080000c2,	/* call &sqrx_exp */
0x080000d5,	/* call &mulx_exp */
0x84004060,	/* ldi r0, [#3] */
0x95800000,	/* lddmp r0 */
0x99080000,	/* strnd r2 */
0x54084200,	/* sub r2, r2, r2 */
0x0600c004,	/* loop *6 ( */
0x99080000,	/* strnd r2 */
0x8c0c1400,	/* ld *3, *4 */
0x50884200,	/* addc r2, r2, r2 */
0x90700300,	/* st *3, *4++ */
/*		   ) */
0x99080000,	/* strnd r2 */
0x8c0c1400,	/* ld *3, *4 */
0x50884200,	/* addc r2, r2, r2 */
0x90700300,	/* st *3, *4++ */
0x0600c008,	/* loop *6 ( */
0x99080000,	/* strnd r2 */
0x8c041500,	/* ld *1, *5 */
0x90540300,	/* st *3, *5 */
0x7c8c0800,	/* ldr *3, *0++ */
0x7c000200,	/* mov r0, r2 */
0x99080000,	/* strnd r2 */
0x64086008,	/* selc r2, r0, r3 */
0x90740300,	/* st *3, *5++ */
/*		   ) */
0xfc000000,	/* nop */
/*		   ) */
0x84004060,	/* ldi r0, [#3] */
0x95800000,	/* lddmp r0 */
0x080000a0,	/* call &mul1_exp */
0x0c000000,	/* ret */
/* } */
/* @0x15a: function modload[12] { */
#define CF_modload_adr 346
0x4c7fff00,	/* xor r31, r31, r31 */
0x84004000,	/* ldi r0, [#0] */
0x95800000,	/* lddmp r0 */
0x94800000,	/* ldlc r0 */
0x8000001c,	/* movi r0.0l, #28 */
0x8080001d,	/* movi r0.1l, #29 */
0x97800000,	/* ldrfp r0 */
0x8c001000,	/* ld *0, *0 */
0x08000001,	/* call &d0inv */
0x90440100,	/* st *1, *1 */
0x08000019,	/* call &computeRR */
0x0c000000,	/* ret */
/* } */
};

struct DMEM_montmul_ptrs {
	uint32_t pMod;
	uint32_t pDinv;
	uint32_t pRR;
	uint32_t pA;
	uint32_t pB;
	uint32_t pC;
	uint32_t n;
	uint32_t n1;
};

/*
 * This struct is "calling convention" for passing parameters into the
 * code block above for RSA operations.  Parameters start at &DMEM[0].
 */
struct DMEM_montmul {
	struct DMEM_montmul_ptrs in_ptrs;
	struct DMEM_montmul_ptrs sqr_ptrs;
	struct DMEM_montmul_ptrs mul_ptrs;
	struct DMEM_montmul_ptrs out_ptrs;
	uint32_t mod[RSA_WORDS_4K];
	uint32_t dInv[8];
	uint32_t pubexp;
	uint32_t _pad1[3];
	uint32_t rnd[2];
	uint32_t _pad2[2];
	uint32_t RR[RSA_WORDS_4K];
	uint32_t in[RSA_WORDS_4K];
	uint32_t exp[RSA_WORDS_4K + 8];  /* extra word for randomization */
	uint32_t out[RSA_WORDS_4K];
	uint32_t bin[RSA_WORDS_4K];
	uint32_t bout[RSA_WORDS_4K];
};

#define DMEM_CELL_SIZE 32
#define DMEM_INDEX(p, f) \
	(((const uint8_t *)&(p)->f - (const uint8_t *)(p)) / DMEM_CELL_SIZE)

#if 1
#define BN_print(...)
#else
static void BN_print(const char* tag, const struct LITE_BIGNUM* n) {
	const uint8_t* p = (const uint8_t*)(n->d + n->dmax);
	size_t i = n->dmax * sizeof(n->d[0]);

	ccprintf("%s:", tag);
	while (i--) {
		ccprintf("%02x", *--p);
	}
	ccprintf("\n");
	cflush();
}
#endif

/* output = input ** exp % N. */
/* TODO(ngm): propagate return code. */
void dcrypto_modexp(struct LITE_BIGNUM *output,
		const struct LITE_BIGNUM *input,
		const struct LITE_BIGNUM *exp,
		const struct LITE_BIGNUM *N,
		uint32_t pubexp) {
	int i, result;
	struct DMEM_montmul *montmul;

	uint32_t r_buf[RSA_MAX_WORDS];
	uint32_t rinv_buf[RSA_MAX_WORDS];

	struct LITE_BIGNUM r;
	struct LITE_BIGNUM rinv;
	struct LITE_BIGNUM pub;

	if (pubexp != 0) {
		bn_init(&r, r_buf, bn_size(N));
		bn_init(&rinv, rinv_buf, bn_size(N));

		/*
		 * pick 64 bit rnd != 0
		 * We cannot tolerate risk of 0 since 0 breaks computation.
		 */
		do {
			BN_DIGIT(&r, 0) = rand();
			BN_DIGIT(&r, 1) = rand();
		} while ((BN_DIGIT(&r, 0) | BN_DIGIT(&r, 1)) == 0);

		BN_print("N", N);
		BN_print("r", &r);

		/*
		 * compute rnd**-1
		 * Note this cannot fail since N is product of two large primes,
		 * so we can ignore return value.
		 */
		bn_modinv_vartime(&rinv, &r, N);

		BN_print("rinv", &rinv);

		/*
		 * compute rnd**pubexp, by recursing.
		 * Note recursion is limited to depth 1 since we pass 0 for pubexp.
		 */
		DCRYPTO_bn_wrap(&pub, &pubexp, sizeof(pubexp));
		dcrypto_modexp(&r, &r, &pub, N, 0);

		BN_print("r**pub", &r);
	}

	/* Initialize hardware; load code page. */
	dcrypto_init_and_lock();
	dcrypto_imem_load(0, IMEM_dcrypto, ARRAY_SIZE(IMEM_dcrypto));

	/* Point to DMEM. */
	montmul = (struct DMEM_montmul *)GREG32_ADDR(CRYPTO, DMEM_DUMMY);

	/* Setup DMEM pointers (as indices into DMEM which are 256-bit cells). */
	montmul->in_ptrs.pMod = DMEM_INDEX(montmul, mod);
	montmul->in_ptrs.pDinv = DMEM_INDEX(montmul, dInv);
	montmul->in_ptrs.pRR = DMEM_INDEX(montmul, RR);
	montmul->in_ptrs.pA = DMEM_INDEX(montmul, in);
	montmul->in_ptrs.pB = DMEM_INDEX(montmul, exp);
	montmul->in_ptrs.pC = DMEM_INDEX(montmul, out);
	montmul->in_ptrs.n = bn_bits(N) / (DMEM_CELL_SIZE * 8);
	montmul->in_ptrs.n1 = montmul->in_ptrs.n - 1;

	montmul->sqr_ptrs = montmul->in_ptrs;
	montmul->mul_ptrs = montmul->in_ptrs;
	montmul->out_ptrs = montmul->in_ptrs;

	dcrypto_dmem_load(DMEM_INDEX(montmul, mod), N->d, bn_words(N));
	dcrypto_dmem_load(DMEM_INDEX(montmul, in), input->d, bn_words(input));

	montmul->pubexp = pubexp;
	montmul->_pad1[0] = montmul->_pad1[1] = montmul->_pad1[2] = 0;
	montmul->rnd[0] = montmul->rnd[1] = 0;
	montmul->_pad2[0] = montmul->_pad2[1] = 0;

	if (pubexp != 0) {
		/* Pick !0 64-bit random */
		do {
			montmul->rnd[0] = rand();
			montmul->rnd[1] = rand();
		} while ((montmul->rnd[0] | montmul->rnd[1]) == 0);

		dcrypto_dmem_load(DMEM_INDEX(montmul, bin), r.d, bn_words(&r));
		dcrypto_dmem_load(DMEM_INDEX(montmul, bout), rinv.d, bn_words(&rinv));
	}

	/* Calculate RR and d0inv. */
	result = dcrypto_call(CF_modload_adr);

	if (bn_words(exp) > 1) {
		dcrypto_dmem_load(DMEM_INDEX(montmul, exp), exp->d, bn_words(exp));

		/* 0 pad the exponent to full size + 8 */
		for (i = bn_words(exp); i < bn_words(N) + 8; ++i)
			montmul->exp[i] = 0;

		BN_print("\nN", N);
		BN_print("in", input);
		BN_print("exp", exp);

		/* in = in * RR */
		montmul->in_ptrs.pA = DMEM_INDEX(montmul, in);
		montmul->in_ptrs.pB = DMEM_INDEX(montmul, RR);
		montmul->in_ptrs.pC = DMEM_INDEX(montmul, in);

		if (pubexp != 0) {
			/* in = in * bin */
			result |= dcrypto_call(CF_mulx_adr);
			montmul->in_ptrs.pB = DMEM_INDEX(montmul, bin);
			result |= dcrypto_call(CF_mulx_adr);
			montmul->in_ptrs.pB = DMEM_INDEX(montmul, RR);
		}

		/* out = out * out */
		montmul->sqr_ptrs.pA = DMEM_INDEX(montmul, out);
		montmul->sqr_ptrs.pB = DMEM_INDEX(montmul, out);
		montmul->sqr_ptrs.pC = DMEM_INDEX(montmul, out);

		/* out = out * in */
		montmul->mul_ptrs.pA = DMEM_INDEX(montmul, in);
		montmul->mul_ptrs.pB = DMEM_INDEX(montmul, out);
		montmul->mul_ptrs.pC = DMEM_INDEX(montmul, out);

		/* out = out / R */
		montmul->out_ptrs.pA = DMEM_INDEX(montmul, out);
		montmul->out_ptrs.pB = DMEM_INDEX(montmul, exp);
		montmul->out_ptrs.pC = DMEM_INDEX(montmul, out);

		if (pubexp != 0) {
			result |= dcrypto_call(CF_modexp_blinded_adr);
			/* remove blinded factor */
			/* out = out * bout */
			montmul->in_ptrs.pA = DMEM_INDEX(montmul, out);
			montmul->in_ptrs.pB = DMEM_INDEX(montmul, RR);
			montmul->in_ptrs.pC = DMEM_INDEX(montmul, out);
			result |= dcrypto_call(CF_mulx_adr);
			montmul->in_ptrs.pB = DMEM_INDEX(montmul, bout);
			result |= dcrypto_call(CF_mulx_adr);
			montmul->in_ptrs.pB = DMEM_INDEX(montmul, RR);
			result |= dcrypto_call(CF_mulx_adr);
			montmul->in_ptrs.pB = DMEM_INDEX(montmul, out);
			result |= dcrypto_call(CF_mul1_adr);
		} else {
			result |= dcrypto_call(CF_modexp_adr);
		}
	} else {
		/* Small public exponent */
		uint32_t e = BN_DIGIT(exp, 0);
		uint32_t b = 0x80000000;

		while (b != 0 && !(b & e))
			b >>= 1;

		/* out = in * RR */
		montmul->in_ptrs.pA = DMEM_INDEX(montmul, in);
		montmul->in_ptrs.pB = DMEM_INDEX(montmul, RR);
		montmul->in_ptrs.pC = DMEM_INDEX(montmul, out);
		result |= dcrypto_call(CF_mulx_adr);

		/* in = in * RR */
		montmul->in_ptrs.pC = DMEM_INDEX(montmul, in);
		result |= dcrypto_call(CF_mulx_adr);

		b >>= 1;

		while (b != 0) {
			/* out = out * out */
			montmul->in_ptrs.pA = DMEM_INDEX(montmul, out);
			montmul->in_ptrs.pB = DMEM_INDEX(montmul, out);
			montmul->in_ptrs.pC = DMEM_INDEX(montmul, out);
			result |= dcrypto_call(CF_mulx_adr);

			if ((b & e) != 0) {
				/* out = out * in */
				montmul->in_ptrs.pA = DMEM_INDEX(montmul, in);
				montmul->in_ptrs.pB = DMEM_INDEX(montmul, out);
				montmul->in_ptrs.pC = DMEM_INDEX(montmul, out);
				result |= dcrypto_call(CF_mulx_adr);
			}

			b >>= 1;
		}
		/* out = out / R */
		montmul->in_ptrs.pA = DMEM_INDEX(montmul, out);
		montmul->in_ptrs.pB = DMEM_INDEX(montmul, out);
		montmul->in_ptrs.pC = DMEM_INDEX(montmul, out);
		result |= dcrypto_call(CF_mul1_adr);
	}

	memcpy(output->d, montmul->out, bn_size(output));

	(void) (result == 0); /* end of errorcode propagation */

	dcrypto_unlock();
}
