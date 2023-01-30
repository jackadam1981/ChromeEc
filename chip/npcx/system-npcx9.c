/* Copyright 2017 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdnoreturn.h>

/* System module driver depends on chip series for Chrome EC */
#include "builtin/assert.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "hwtimer_chip.h"
#include "mpu.h"
#include "lct_chip.h"
#include "registers.h"
#include "rom_chip.h"
#include "system.h"
#include "system_chip.h"
#include "task.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)

/* Macros for last 32K ram block */
#define LAST_RAM_BLK ((NPCX_RAM_SIZE / (32 * 1024)) - 1)
/* Higher bits are reserved and need to be masked */
#define RAM_PD_MASK (~BIT(LAST_RAM_BLK))

/* Begin address of Suspend RAM for hibernate utility */
uintptr_t __lpram_fw_start = CONFIG_LPRAM_BASE;
/* Offset of little FW in Suspend Ram for GDMA bypass */
#define LFW_OFFSET 0x160
/* Begin address of Suspend RAM for little FW (GDMA utilities). */
uintptr_t __lpram_lfw_start = CONFIG_LPRAM_BASE + LFW_OFFSET;

#ifdef CONFIG_EXTERNAL_STORAGE

#define chunkSize 16 /* 4 data burst mode. ie.16 bytes */
#define DMA_SIZE 256

uint8_t decompress_buf[DMA_SIZE] __aligned(16);


static inline __attribute__((always_inline)) uint16_t LZ4_readLE16(const void *src)
{
	        return (uint16_t)(*(const uint16_t *)src);
}

static inline __attribute__((always_inline)) void LZ4_copy8(void *dst, const void *src)
{
	int i;
	for (i = 0; i < 8; i++)
		((uint8_t *)dst)[i] = ((uint8_t *)src)[i];
}

typedef  uint8_t BYTE;
typedef uint16_t U16;
typedef uint32_t U32;
typedef  int32_t S32;
typedef uint64_t U64;

#define FORCE_INLINE static __always_inline
#define likely(expr) __builtin_expect((expr) != 0, 1)
#define unlikely(expr) __builtin_expect((expr) != 0, 0)

/* Unaltered (just removed unrelated code) from github.com/Cyan4973/lz4/dev. */
/**************************************
*  Reading and writing into memory
**************************************/

/* customized variant of memcpy, which can overwrite up to 7 bytes beyond dstEnd */
static inline __attribute__((always_inline)) void LZ4_wildCopy(void* dstPtr, const void* srcPtr, void* dstEnd)
{
    BYTE* d = (BYTE*)dstPtr;
    const BYTE* s = (const BYTE*)srcPtr;
    BYTE* const e = (BYTE*)dstEnd;

    do { LZ4_copy8(d,s); d+=8; s+=8; } while (d<e);
}


/**************************************
*  Common Constants
**************************************/
#define MINMATCH 4

#define WILDCOPYLENGTH 8
#define LASTLITERALS 5
#define MFLIMIT (WILDCOPYLENGTH+MINMATCH)

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define MAXD_LOG 16
#define MAX_DISTANCE ((1 << MAXD_LOG) - 1)

#define ML_BITS  4
#define ML_MASK  ((1U<<ML_BITS)-1)
#define RUN_BITS (8-ML_BITS)
#define RUN_MASK ((1U<<RUN_BITS)-1)


/**************************************
*  Local Structures and types
**************************************/
typedef enum { noDict = 0, withPrefix64k, usingExtDict } dict_directive;
typedef enum { endOnOutputSize = 0, endOnInputSize = 1 } endCondition_directive;
typedef enum { full = 0, partial = 1 } earlyEnd_directive;


/*******************************
*  Decompression functions
*******************************/
/*
 * This generic decompression function cover all use cases.
 * It shall be instantiated several times, using different sets of directives
 * Note that it is essential this generic function is really inlined,
 * in order to remove useless branches during compilation optimization.
 */
static inline __attribute__((always_inline)) int LZ4_decompress_generic(
                 const char* const source,
                 char* const dest,
                 int inputSize,
                 int outputSize,         /* If endOnInput==endOnInputSize, this value is the max size of Output Buffer. */

                 int endOnInput,         /* endOnOutputSize, endOnInputSize */
                 int partialDecoding,    /* full, partial */
                 int targetOutputSize,   /* only used if partialDecoding==partial */
                 int dict,               /* noDict, withPrefix64k, usingExtDict */
                 const BYTE* const lowPrefix,  /* == dest if dict == noDict */
                 const BYTE* const dictStart,  /* only if dict==usingExtDict */
                 const size_t dictSize         /* note : = 0 if noDict */
                 )
{
    /* Local Variables */
    const BYTE* ip = (const BYTE*) source;
    const BYTE* const iend = ip + inputSize;

    BYTE* op = (BYTE*) dest;
    BYTE* const oend = op + outputSize;
    BYTE* cpy;
    BYTE* oexit = op + targetOutputSize;
    const BYTE* const lowLimit = lowPrefix - dictSize;

    const BYTE* const dictEnd = (const BYTE*)dictStart + dictSize;
    const unsigned dec32table[] = {4, 1, 2, 1, 4, 4, 4, 4};
    const int dec64table[] = {0, 0, 0, -1, 0, 1, 2, 3};

    const int safeDecode = (endOnInput==endOnInputSize);
    const int checkOffset = ((safeDecode) && (dictSize < (int)(64 KB)));
    const int inPlaceDecode = ((ip >= op) && (ip < oend));


    /* Special cases */
    if ((partialDecoding) && (oexit> oend-MFLIMIT)) oexit = oend-MFLIMIT;                         /* targetOutputSize too high => decode everything */
    if ((endOnInput) && (unlikely(outputSize==0))) return ((inputSize==1) && (*ip==0)) ? 0 : -1;  /* Empty output buffer */
    if ((!endOnInput) && (unlikely(outputSize==0))) return (*ip==0?1:-1);


    /* Main Loop */
    while (1)
    {
        unsigned token;
        size_t length;
        const BYTE* match;
        size_t offset;

        if (unlikely((inPlaceDecode) && (op + WILDCOPYLENGTH > ip))) goto _output_error;   /* output stream ran over input stream */

        /* get literal length */
        token = *ip++;
        if ((length=(token>>ML_BITS)) == RUN_MASK)
        {
            unsigned s;
            if ((endOnInput) && unlikely(ip>=iend-RUN_MASK)) goto _output_error;   /* overflow detection */
            do
            {
                s = *ip++;
                length += s;
            }
            while ( likely(endOnInput ? ip<iend-RUN_MASK : 1) && (s==255) );
            if ((safeDecode) && unlikely((size_t)(op+length)<(size_t)(op))) goto _output_error;   /* overflow detection */
            if ((safeDecode) && unlikely((size_t)(ip+length)<(size_t)(ip))) goto _output_error;   /* overflow detection */
        }

        /* copy literals */
        cpy = op+length;
        if (((endOnInput) && ((cpy>(partialDecoding?oexit:oend-MFLIMIT)) || (ip+length>iend-(2+1+LASTLITERALS))) )
            || ((!endOnInput) && (cpy>oend-WILDCOPYLENGTH)))
        {
            if (partialDecoding)
            {
                if (cpy > oend) goto _output_error;                           /* Error : write attempt beyond end of output buffer */
                if ((endOnInput) && (ip+length > iend)) goto _output_error;   /* Error : read attempt beyond end of input buffer */
            }
            else
            {
                if ((!endOnInput) && (cpy != oend)) goto _output_error;       /* Error : block decoding must stop exactly there */
                if ((endOnInput) && ((ip+length != iend) || (cpy > oend))) goto _output_error;   /* Error : input must be consumed */
            }
            memmove(op, ip, length);
            ip += length;
            op += length;
            break;     /* Necessarily EOF, due to parsing restrictions */
        }
        LZ4_wildCopy(op, ip, cpy);
        ip += length; op = cpy;

        /* get offset */
        offset = LZ4_readLE16(ip); ip+=2;
        match = op - offset;
        if ((checkOffset) && (unlikely(match < lowLimit))) goto _output_error;   /* Error : offset outside buffers */

        /* get matchlength */
        length = token & ML_MASK;
        if (length == ML_MASK)
        {
            unsigned s;
            do
            {
                if ((endOnInput) && (ip > iend-LASTLITERALS)) goto _output_error;
                s = *ip++;
                length += s;
            } while (s==255);
            if ((safeDecode) && unlikely((size_t)(op+length)<(size_t)op)) goto _output_error;   /* overflow detection */
        }
        length += MINMATCH;

        /* check external dictionary */
        if ((dict==usingExtDict) && (match < lowPrefix))
        {
            if (unlikely(op+length > oend-LASTLITERALS)) goto _output_error;   /* doesn't respect parsing restriction */

            if (length <= (size_t)(lowPrefix-match))
            {
                /* match can be copied as a single segment from external dictionary */
                match = dictEnd - (lowPrefix-match);
                memmove(op, match, length); op += length;
            }
            else
            {
                /* match encompass external dictionary and current block */
                size_t copySize = (size_t)(lowPrefix-match);
                memcpy(op, dictEnd - copySize, copySize);
                op += copySize;
                copySize = length - copySize;
                if (copySize > (size_t)(op-lowPrefix))   /* overlap copy */
                {
                    BYTE* const endOfMatch = op + copySize;
                    const BYTE* copyFrom = lowPrefix;
                    while (op < endOfMatch) *op++ = *copyFrom++;
                }
                else
                {
                    memcpy(op, lowPrefix, copySize);
                    op += copySize;
                }
            }
            continue;
        }

        /* copy match within block */
        cpy = op + length;
        if (unlikely(offset<8))
        {
            const int dec64 = dec64table[offset];
            op[0] = match[0];
            op[1] = match[1];
            op[2] = match[2];
            op[3] = match[3];
            match += dec32table[offset];
            memcpy(op+4, match, 4);
            match -= dec64;
        } else { LZ4_copy8(op, match); match+=8; }
        op += 8;

        if (unlikely(cpy>oend-12))
        {
            BYTE* const oCopyLimit = oend-(WILDCOPYLENGTH-1);
            if (cpy > oend-LASTLITERALS) goto _output_error;    /* Error : last LASTLITERALS bytes must be literals (uncompressed) */
            if (op < oCopyLimit)
            {
                LZ4_wildCopy(op, match, oCopyLimit);
                match += oCopyLimit - op;
                op = oCopyLimit;
            }
            while (op<cpy) *op++ = *match++;
        }
        else
            LZ4_wildCopy(op, match, cpy);
        op=cpy;   /* correction */
    }

    /* end of decoding */
    if (endOnInput)
       return (int) (((char*)op)-dest);     /* Nb of output bytes decoded */
    else
       return (int) (((const char*)ip)-source);   /* Nb of input bytes read */

    /* Overflow error detected */
_output_error:
    return (int) (-(((const char*)ip)-source))-1;
}

#define LZ4F_MAGICNUMBER 0x184D2204

/* Bit field endianness is implementation-defined. Use masks instead.
 * https://stackoverflow.com/a/6044223 */
#define RESERVED0		0x03
#define HAS_CONTENT_CHECKSUM	0x04
#define HAS_CONTENT_SIZE	0x08
#define HAS_BLOCK_CHECKSUM	0x10
#define INDEPENDENT_BLOCKS	0x20
#define VERSION			0xC0
#define VERSION_SHIFT		6

#define RESERVED1_2		0x8F
#define MAX_BLOCK_SIZE		0x70

struct lz4_frame_header {
	uint32_t magic;
	uint8_t flags;
	uint8_t block_descriptor;
	/* + uint64_t content_size iff has_content_size is set */
	/* + uint8_t header_checksum */
} __packed;

#define BH_SIZE			0x7FFFFFFF
#define NOT_COMPRESSED		0x80000000

struct lz4_block_header {
	uint32_t raw;
	/* + size bytes of data */
	/* + uint32_t block_checksum iff has_block_checksum is set */
};

static inline __attribute__((always_inline))
size_t ulz4fn(const void *src, size_t srcn, void *dst, size_t dstn)
{
	const void *in = src;
	void *out = dst;
	size_t out_size = 0;
	int has_block_checksum;

	{ /* With in-place decompression the header may become invalid later. */
		const struct lz4_frame_header *h = in;

		if (srcn < sizeof(*h) + sizeof(uint64_t) + sizeof(uint8_t))
			return 0;	/* input overrun */

		/* We assume there's always only a single, standard frame. */
		if ((uint32_t)(h->magic) != LZ4F_MAGICNUMBER
		    || (h->flags & VERSION) != (1 << VERSION_SHIFT))
			return 0;	/* unknown format */
		if ((h->flags & RESERVED0) || (h->block_descriptor & RESERVED1_2))
			return 0;	/* reserved must be zero */
		if (!(h->flags & INDEPENDENT_BLOCKS))
			return 0;	/* we don't support block dependency */
		has_block_checksum = h->flags & HAS_BLOCK_CHECKSUM;

		in += sizeof(*h);
		if (h->flags & HAS_CONTENT_SIZE)
			in += sizeof(uint64_t);
		in += sizeof(uint8_t);
	}

	while (1) {
		struct lz4_block_header b = {
			.raw = (uint32_t)(*(const uint32_t *)in)
		};

		if ((size_t)(in - src) + sizeof(struct lz4_block_header) > srcn)
			break;          /* input overrun */

		in += sizeof(struct lz4_block_header);

		if ((size_t)(in - src) + (b.raw & BH_SIZE) > srcn)
			break;			/* input overrun */

		if (!(b.raw & BH_SIZE)) {
			out_size = out - dst;
			break;			/* decompression successful */
		}

		if (b.raw & NOT_COMPRESSED) {
			size_t size = MIN((uintptr_t)(b.raw & BH_SIZE), (uintptr_t)dst
				+ dstn - (uintptr_t)out);
			memcpy(out, in, size);
			if (size < (b.raw & BH_SIZE))
				break;		/* output overrun */
			out += size;
		} else {
			/* constant folding essential, do not touch params! */
			int ret = LZ4_decompress_generic(in, out, (b.raw & BH_SIZE),
					dst + dstn - out, endOnInputSize,
					full, 0, noDict, out, NULL, 0);
			if (ret < 0)
				break;		/* decompression error */
			out += ret;
		}

		in += (b.raw & BH_SIZE);
		if (has_block_checksum)
			in += sizeof(uint32_t);
	}

	return out_size;
}

static inline __attribute__((always_inline))
size_t __decompress(uint32_t srcAddr, uint32_t dstAddr, uint32_t size)
{
#if 0
	uint32_t j;
	uint32_t *srcPtr, *dstPtr;

	srcPtr = (uint32_t *)srcAddr;
	dstPtr = (uint32_t *)dstAddr;
	for (j = 0; j < size / 4; j++, dstPtr++, srcPtr++)
		*dstPtr = *srcPtr;

	return size;
#else

	return ulz4fn((void *)srcAddr, size, (void *)dstAddr, 0x3afa0);

#endif
}

/* Sysjump utilities in low power ram for npcx5 series. */
noreturn void __keep __attribute__((section(".lowpower_ram2")))
__start_gdma_and_jump(uint32_t srcAddr, uint32_t dstAddr,
		      uint32_t size, uint32_t exeAddr)
{
	uint32_t src, dst;

	/*
	 * Initialize GDMA for flash reading.
	 * [31:21] - Reserved.
	 * [20]    - GDMAERR   = 0  (Indicate GMDA transfer error)
	 * [19]    - Reserved.
	 * [18]    - TC        = 0  (Terminal Count. Indicate operation is end.)
	 * [17]    - Reserved.
	 * [16]    - SOFTREQ   = 0  (Don't trigger here)
	 * [15]    - DM        = 0  (Set normal demand mode)
	 * [14]    - Reserved.
	 * [13:12] - TWS.      = 10 (One double-word for every GDMA transaction)
	 * [11:10] - Reserved.
	 * [9]     - BME       = 1  (4-data ie.16 bytes - Burst mode enable)
	 * [8]     - SIEN      = 0  (Stop interrupt disable)
	 * [7]     - SAFIX     = 0  (Fixed source address)
	 * [6]     - Reserved.
	 * [5]     - SADIR     = 0  (Source address incremented)
	 * [4]     - DADIR     = 0  (Destination address incremented)
	 * [3:2]   - GDMAMS    = 00 (Software mode)
	 * [1]     - Reserved.
	 * [0]     - ENABLE    = 0  (Don't enable yet)
	 */

	src = CONFIG_MAPPED_STORAGE_BASE + srcAddr;
	dst = dstAddr;

#if 0
	for (uint32_t i = 0; i < size / DMA_SIZE; i++, src+=DMA_SIZE, dst+=DMA_SIZE) {
		NPCX_GDMA_CTL = 0x00002200;

		/* Set source base address */
		NPCX_GDMA_SRCB = src;

		/* Set destination base address */
		NPCX_GDMA_DSTB = (uint32_t)decompress_buf;

		/* Set number of transfers */
		NPCX_GDMA_TCNT = (DMA_SIZE / chunkSize);

		/* Clear Transfer Complete event */
		SET_BIT(NPCX_GDMA_CTL, NPCX_GDMA_CTL_TC);

		/* Enable GDMA now */
		SET_BIT(NPCX_GDMA_CTL, NPCX_GDMA_CTL_GDMAEN);

		/* Start GDMA */
		SET_BIT(NPCX_GDMA_CTL, NPCX_GDMA_CTL_SOFTREQ);

		/* Wait for transfer to complete/fail */
		while (!IS_BIT_SET(NPCX_GDMA_CTL, NPCX_GDMA_CTL_TC) &&
		       !IS_BIT_SET(NPCX_GDMA_CTL, NPCX_GDMA_CTL_GDMAERR))
			;

		/* Disable GDMA now */
		CLEAR_BIT(NPCX_GDMA_CTL, NPCX_GDMA_CTL_GDMAEN);

		/*
		 * Failure occurs during GMDA transaction. Let watchdog issue and
		 * boot from RO region again.
		 */
		if (IS_BIT_SET(NPCX_GDMA_CTL, NPCX_GDMA_CTL_GDMAERR))
			while (1)
				;

		__decompress((uint32_t)decompress_buf, dst, DMA_SIZE);
	}
#endif

	if (!__decompress(src, dst, size))
		while (1)
			;

	/*
	 * Jump to the exeAddr address if needed. Setting bit 0 of address to
	 * indicate it's a thumb branch for cortex-m series CPU.
	 */
	/*
	((void (*)(void))(exeAddr | 0x01))();
	*/

	/* Should never get here */
	while (1)
		;
}

/* Bypass for GMDA issue of ROM api utilities only on npcx5 series. */
void system_download_from_flash(uint32_t srcAddr, uint32_t dstAddr,
				uint32_t size, uint32_t exeAddr)
{
	int i;
	/*
	 * GDMA utility in Suspend RAM. Setting bit 0 of address to indicate
	 * it's a thumb branch for cortex-m series CPU.
	 */
	void (*__start_gdma_in_lpram)(uint32_t, uint32_t, uint32_t, uint32_t) =
		(void (*)(uint32_t, uint32_t, uint32_t, uint32_t))(__lpram_lfw_start | 0x01);

	/*
	 * Before enabling burst mode for better performance of GDMA, it's
	 * important to make sure srcAddr, dstAddr and size of transactions
	 * are 16 bytes aligned in case failure occurs.
	 */
	ASSERT((size % chunkSize) == 0 && (srcAddr % chunkSize) == 0 &&
	       (dstAddr % chunkSize) == 0);

	/* Check valid address for jumpiing */
	ASSERT(exeAddr != 0x0);

	/* Enable power for the Low Power RAM */
	CLEAR_BIT(NPCX_PWDWN_CTL(NPCX_PMC_PWDWN_6), 6);

	/* Enable Low Power RAM */
	NPCX_LPRAM_CTRL = 1;

	/* Copy the __start_gdma_in_lpram instructions to LPRAM */
	for (i = 0; i < &__flash_lplfw_end - &__flash_lplfw_start; i++)
		*((uint32_t *)__lpram_lfw_start + i) =
			*(&__flash_lplfw_start + i);

	/* Start GDMA in Suspend RAM */
	__start_gdma_in_lpram(srcAddr, dstAddr, size, exeAddr);
}
#endif /* CONFIG_EXTERNAL_STORAGE */

/*****************************************************************************/
/* IC specific low-level driver depends on chip series */

/**
 * Configure address 0x40001600 (Low Power RAM) in the the MPU
 * (Memory Protection Unit) as a "regular" memory
 */
void system_mpu_config(void)
{
	/* Enable MPU */
	CPU_MPU_CTRL = 0x7;

	/* Create a new MPU Region to allow execution from low-power ram */
	CPU_MPU_RNR = REGION_CHIP_RESERVED;
	CPU_MPU_RASR = CPU_MPU_RASR & 0xFFFFFFFE; /* Disable region */
	CPU_MPU_RBAR = CONFIG_LPRAM_BASE; /* Set region base address */
	/*
	 * Set region size & attribute and enable region
	 * [31:29] - Reserved.
	 * [28]    - XN (Execute Never) = 0
	 * [27]    - Reserved.
	 * [26:24] - AP                 = 011 (Full access)
	 * [23:22] - Reserved.
	 * [21:19,18,17,16] - TEX,S,C,B = 001000 (Normal memory)
	 * [15:8]  - SRD                = 0 (Subregions enabled)
	 * [7:6]   - Reserved.
	 * [5:1]   - SIZE               = 01001 (1K)
	 * [0]     - ENABLE             = 1 (enabled)
	 */
	CPU_MPU_RASR = 0x03080013;
}

#ifdef CONFIG_HIBERNATE_PSL
#ifndef NPCX_PSL_MODE_SUPPORT
#error "Do not enable CONFIG_HIBERNATE_PSL if npcx ec doesn't support PSL mode!"
#endif

static enum psl_pin_t system_gpio_to_psl(enum gpio_signal signal)
{
	enum psl_pin_t psl_no;
	const struct gpio_info *g = gpio_list + signal;

	if (g->port == GPIO_PORT_D && g->mask == MASK_PIN2) /* GPIOD2 */
		psl_no = PSL_IN1;
	else if (g->port == GPIO_PORT_0 && (g->mask & 0x07)) /* GPIO00/01/02 */
		psl_no = GPIO_MASK_TO_NUM(g->mask) + 1;
	else
		psl_no = PSL_NONE;

	return psl_no;
}

#if NPCX_FAMILY_VERSION >= NPCX_FAMILY_NPCX9
void system_set_psl_gpo(int level)
{
	if (level)
		SET_BIT(NPCX_GLUE_PSL_MCTL1, NPCX_GLUE_PSL_MCTL1_PSL_GPO_CTL);
	else
		CLEAR_BIT(NPCX_GLUE_PSL_MCTL1, NPCX_GLUE_PSL_MCTL1_PSL_GPO_CTL);
}
#endif

void system_enter_psl_mode(void)
{
	/* Configure pins from GPIOs to PSL which rely on VSBY power rail. */
	gpio_config_module(MODULE_PMU, 1);

	/*
	 * In npcx7, only physical PSL_IN pins can pull PSL_OUT to high and
	 * reboot ec.
	 * In npcx9, LCT timeout event can also pull PSL_OUT.
	 * We won't decide the wake cause now but only mark we are entering
	 * hibernation via PSL.
	 * The actual wakeup cause will be checked by the PSL input event bits
	 * when ec reboots.
	 */
	NPCX_BBRAM(BBRM_DATA_INDEX_WAKE) = HIBERNATE_WAKE_PSL;

#if NPCX_FAMILY_VERSION >= NPCX_FAMILY_NPCX9
	/*
	 * If pulse mode is enabled, the VCC power is turned off by the
	 * external component (Ex: PMIC) but PSL_OUT. So we can just return
	 * here.
	 */
	if (IS_BIT_SET(NPCX_GLUE_PSL_MCTL1, NPCX_GLUE_PSL_MCTL1_PLS_EN))
		return;
#endif

	/*
	 * Pull PSL_OUT (GPIO85) to low to cut off ec's VCC power rail by
	 * setting bit 5 of PDOUT(8).
	 */
	SET_BIT(NPCX_PDOUT(GPIO_PORT_8), 5);
}

/* Hibernate function implemented by PSL (Power Switch Logic) mode. */
noreturn void __keep __enter_hibernate_in_psl(void)
{
	system_enter_psl_mode();
	/* Spin and wait for PSL cuts power; should never return */
	while (1)
		;
}

static void system_psl_type_sel(enum psl_pin_t psl_pin, uint32_t flags)
{
	/* Set PSL input events' type as level or edge trigger */
	if ((flags & GPIO_INT_F_HIGH) || (flags & GPIO_INT_F_LOW))
		CLEAR_BIT(NPCX_GLUE_PSL_CTS, psl_pin + 4);
	else if ((flags & GPIO_INT_F_RISING) || (flags & GPIO_INT_F_FALLING))
		SET_BIT(NPCX_GLUE_PSL_CTS, psl_pin + 4);

	/*
	 * Set PSL input events' polarity is low (high-to-low) active or
	 * high (low-to-high) active
	 */
	if (flags & GPIO_HIB_WAKE_HIGH)
		SET_BIT(NPCX_DEVALT(ALT_GROUP_D), 2 * psl_pin);
	else
		CLEAR_BIT(NPCX_DEVALT(ALT_GROUP_D), 2 * psl_pin);
}

int system_config_psl_mode(enum gpio_signal signal)
{
	enum psl_pin_t psl_no;
	const struct gpio_info *g = gpio_list + signal;

	psl_no = system_gpio_to_psl(signal);
	if (psl_no == PSL_NONE)
		return 0;

	system_psl_type_sel(psl_no, g->flags);
	return 1;
}

#else
/**
 * Hibernate function in last 32K ram block for npcx7 series.
 * Do not use global variable since we also turn off data ram.
 */
noreturn void __keep __attribute__((section(".after_init")))
__enter_hibernate_in_last_block(void)
{
	/*
	 * The hibernate utility is located in the last block of RAM. The size
	 * of each RAM block is 32KB. We turn off all blocks except last one
	 * for better power consumption.
	 */
	NPCX_RAM_PD(0) = RAM_PD_MASK & 0xFF;
#if defined(CHIP_FAMILY_NPCX7)
	NPCX_RAM_PD(1) = (RAM_PD_MASK >> 8) & 0x0F;
#elif defined(CHIP_FAMILY_NPCX9)
	NPCX_RAM_PD(1) = (RAM_PD_MASK >> 8) & 0x7F;
#endif

	/* Set deep idle mode */
	NPCX_PMCSR = 0x6;

	/* Enter deep idle, wake-up by GPIOs or RTC */
	asm volatile("wfi");

	/* RTC wake-up */
	if (IS_BIT_SET(NPCX_WTC, NPCX_WTC_PTO))
		/*
		 * Mark wake-up reason for hibernate
		 * Do not call bbram_data_write directly cause of
		 * no stack.
		 */
		NPCX_BBRAM(BBRM_DATA_INDEX_WAKE) = HIBERNATE_WAKE_MTC;
#ifdef NPCX_LCT_SUPPORT
	else if (IS_BIT_SET(NPCX_LCTSTAT, NPCX_LCTSTAT_EVST)) {
		NPCX_BBRAM(BBRM_DATA_INDEX_WAKE) = HIBERNATE_WAKE_LCT;
		/* Clear LCT event */
		NPCX_LCTSTAT = BIT(NPCX_LCTSTAT_EVST);
	}
#endif
	else
		/* Otherwise, we treat it as GPIOs wake-up */
		NPCX_BBRAM(BBRM_DATA_INDEX_WAKE) = HIBERNATE_WAKE_PIN;

	/* Start a watchdog reset */
	NPCX_WDCNT = 0x01;
	/* Reload and restart Timer 0 */
	SET_BIT(NPCX_T0CSR, NPCX_T0CSR_RST);
	/* Wait for timer is loaded and restart */
	while (IS_BIT_SET(NPCX_T0CSR, NPCX_T0CSR_RST))
		;

	/* Spin and wait for reboot; should never return */
	while (1)
		;
}
#endif

/**
 * Hibernate function for different Nuvoton chip series.
 */
void __hibernate_npcx_series(void)
{
#ifdef CONFIG_HIBERNATE_PSL
	__enter_hibernate_in_psl();
#else
	/* Make sure this is located in the last 32K code RAM block */
	ASSERT((uint32_t)(&__after_init_end) - CONFIG_PROGRAM_MEMORY_BASE <
	       (32 * 1024));

	/* Execute hibernate func in last 32K block */
	__enter_hibernate_in_last_block();
#endif
}

#if defined(CONFIG_HIBERNATE_PSL)
static void report_psl_wake_source(void)
{
	if (!(system_get_reset_flags() & EC_RESET_FLAG_HIBERNATE))
		return;

	CPRINTS("PSL_CTS: 0x%x", NPCX_GLUE_PSL_CTS & 0xf);
#if NPCX_FAMILY_VERSION >= NPCX_FAMILY_NPCX9
	CPRINTS("PSL_MCTL1 event: 0x%x", NPCX_GLUE_PSL_MCTL1 & 0x18);
#endif
}
DECLARE_HOOK(HOOK_INIT, report_psl_wake_source, HOOK_PRIO_DEFAULT);
#endif
