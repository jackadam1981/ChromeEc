/*****************************************************************************
* Copyright (c) 2016 Microchip Technology Inc. and its subsidiaries.
* You may use this software and any derivatives exclusively with
* Microchip products.
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS".
* NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
* INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
* AND FITNESS FOR A PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP
* PRODUCTS, COMBINATION WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.
* TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL
* CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF
* FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
* MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE
* OF THESE TERMS.
*****************************************************************************/

/** @file sha256.c
 *MEC1701 Hash hardware accelerators
 */
/** @defgroup MEC1701 Peripherals SHA-256
 *  @{
 */

#include "common.h"
#include "timer.h"
/* #include "task.h" */
#include "registers.h"
#include "sha256_chip.h"


#define ZERO_ALL true
#define ZERO_FROM_REMAINING false


#define BYTE_REVERSE_WORD(w) ((((uint32_t)(w)>>24)&0xFFul) + \
				(((uint32_t)(w)&0x00FF0000ul)>>8) + \
				(((uint32_t)(w)&0x0000FF00ul)<<8) + \
				(((uint32_t)(w)&0xFFul)<<24))


/**
 * @brief SHA256 Initial Value H0 - H7 from FIPS 180-3 Final.
 *        The Hash accelerator requires least signficant first
 *        byte order. Use an ugly C preprocessor macro to
 *        reverse the byte order. The FIPS documents describe
 *        all hash data in human readable left-to-write
 *        (left=most significant) bit order.
 * H0 = 6a09e667
 * H1 = bb67ae85
 * H2 = 3c6ef372
 * H3 = a54ff53a
 * H4 = 510e527f
 * H5 = 9b05688c
 * H6 = 1f83d9ab
 * H7 = 5be0cd19
 *
 */
const uint32_t sha256_init_byte_rev[8] =
{
	BYTE_REVERSE_WORD(0x6a09e667),
	BYTE_REVERSE_WORD(0xbb67ae85),
	BYTE_REVERSE_WORD(0x3c6ef372),
	BYTE_REVERSE_WORD(0xa54ff53a),
	BYTE_REVERSE_WORD(0x510e527f),
	BYTE_REVERSE_WORD(0x9b05688c),
	BYTE_REVERSE_WORD(0x1f83d9ab),
	BYTE_REVERSE_WORD(0x5be0cd19)
};


/*--------------------------------------------------------------------------*/

static void crypto_memset(void * dest, uint32_t val, uint32_t len)
{
	if (NULL != dest) {
		if (0 == (((uint32_t)dest | len) & 0x03ul)) {
			while (len) {
				*(uint32_t *)dest++ = val;
				len -= 4;
			}
		} else {
			while (len) {
				*(uint8_t *)dest++ = (uint8_t)val;
				len--;
			}
		}
	}
}
/*--------------------------------------------------------------------------*/


static void crypto_memcpy(void * dest, const void * src, uint32_t len)
{
	if ((NULL != dest) && (NULL != src)) {
		if (0 == (((uint32_t)dest | (uint32_t)src | len) & 0x03)) {
			while (len) {
				*(uint32_t *)dest++ = *(const uint32_t *)src++;
				len -= 4;
			}
		} else {
			while (len) {
				*(uint8_t *)dest++ = *(const uint8_t *)src++;
				len--;
			}
		}
	}
}
/*--------------------------------------------------------------------------*/

static void sha256_msg_bit_len(uint32_t *pblock, uint32_t msg_len)
{
	uint64_t msg_bit_len;
	uint8_t *pdest;
	uint8_t *psrc;
	uint32_t i;

	if (NULL != pblock) {
		msg_bit_len = (uint64_t)msg_len << 3;
		psrc = (uint8_t *)&msg_bit_len;
		pdest = (uint8_t *)pblock;
		pdest += 63;
		for (i = 0; i < 8; i++) {
			*pdest-- = *psrc++;
		}
	}
}
/*--------------------------------------------------------------------------*/

static uint32_t sha256_copy2block(struct sha256_ctx *pctx,
				  const uint8_t *pdata, uint32_t data_len)
{
	uint32_t n = 0;
	uint8_t * p8 = NULL;

	if (data_len) {
		n = (SHA256_BLOCK_BYTELEN) - pctx->block_len;
		if (data_len <= n) {
			n = data_len;
		}

		p8 = pctx->block.b;
		p8 += pctx->block_len;
		crypto_memcpy(p8, pdata, n);

		pctx->block_len += n;
	}

	return n;
}
/*--------------------------------------------------------------------------*/

/*
 * Wait for SHA-256 hardware to complete for one block.
 * MEC1701 AHB clock = 48MHz (~ 20ns)
 * SHA HW operation is ~ 1byte/clock
 * SHA-256 block size is 64 bytes.
 * Double it for margin and other bus masters might be active.
 * 128 clocks = 2.6 us
 */
#if 0
static int sha_wait(void)
{
	timestamp_t deadline;

	deadline.val = get_time().val +
		(1 * MSEC);
	while (0 != (MEC17XX_SHA_CTRL & MEC17XX_SHA_CTRL_START)) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(4);
	}
	return EC_SUCCESS;
}
#endif
/*--------------------------------------------------------------------------*/

/*
 * Wait for SHA-256 hardware to complete within specified time interval in
 * milliseconds.
 *
 * MEC1701 AHB clock = 48MHz (~ 20ns)
 * SHA HW operation is ~ 1byte/clock
 * SHA-256 block size is 64 bytes.
 * Double it for margin and other bus masters might be active.
 * 128 clocks = 2.6 us
 */
static int sha_wait2(uint32_t nus)
{
	timestamp_t deadline;

	deadline.val = get_time().val + nus;

	while (0 != (MEC17XX_SHA_CTRL & MEC17XX_SHA_CTRL_START)) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(4);
	}
	return EC_SUCCESS;
}
/*--------------------------------------------------------------------------*/

static void sha_start(uint8_t ien)
{
	MEC17XX_INT_DISABLE(MEC17XX_SHA_GIRQ) = MEC17XX_SHA_GIRQ_BIT;
	MEC17XX_INT_SOURCE(MEC17XX_SHA_GIRQ) = MEC17XX_SHA_GIRQ_BIT;

#if 0
	task_disable_irq(MEC17XX_IRQ_HASH);
	task_clear_pending_irq(MEC17XX_IRQ_HASH);
#endif
	if (ien) {
		MEC17XX_INT_ENABLE(MEC17XX_SHA_GIRQ) = MEC17XX_SHA_GIRQ_BIT;
#if 0
		task_enable_irq(MEC17XX_IRQ_HASH);
#endif
	}

	MEC17XX_SHA_CTRL = MEC17XX_SHA_CTRL_START;
}
/*--------------------------------------------------------------------------*/

/*
 * Public API
 */

/**
 * @brief sha_power
 * @param pwr_on 0 power off block, 1 power on
 * @note Control clock gating to SHA and AES block.
 * SHA and AES block share the same AHB master.
 */
void sha_aes_power(int pwr_on)
{
	if (pwr_on) {
		MEC17XX_PCR_SLP_DIS_DEV(MEC17XX_SHA_PCR_SLP_EN_IDX,\
					MEC17XX_SHA_PCR_SLP_EN_BITPOS);
	} else {
		MEC17XX_PCR_SLP_EN_DEV(MEC17XX_SHA_PCR_SLP_EN_IDX,\
					MEC17XX_SHA_PCR_SLP_EN_BITPOS);
	}
}

/**
 * @brief sha_aes_reset
 * @note Soft reset of both SHA and AES hardware blocks. All
 * registers returned to POR state. Clears GIRQ16 Enable and
 * Source bits. Clears NVIC direct enable and pending bits.
 */
void sha_aes_reset(void)
{
	MEC17XX_EC_CRYPTO_SRESET = MEC17XX_CRYPTO_AES_SHA_SRST;
	MEC17XX_EC_CRYPTO_SRESET;
	MEC17XX_INT_DISABLE(MEC17XX_SHA_GIRQ) = MEC17XX_SHA_GIRQ_BIT;
	MEC17XX_INT_SOURCE(MEC17XX_SHA_GIRQ) = MEC17XX_SHA_GIRQ_BIT;
#if 0
	task_disable_irq(MEC17XX_IRQ_HASH);
	task_clear_pending_irq(MEC17XX_IRQ_HASH);
#endif
}

/**
 * @brief sha_busy
 * @return EC_ERROR_BUSY or EC_SUCCESS (not busy)
 */
int sha_busy(void)
{
	if (MEC17XX_SHA_CTRL & MEC17XX_SHA_CTRL_START) {
		return EC_ERROR_BUSY;
	}
	return EC_SUCCESS;
}
/*--------------------------------------------------------------------------*/


/**
 * sha256_init -Initialize SHA-256 digest and hardware
 * @param pointer to 32 byte buffer aligned >= 4-bytes used for
 * initial and final SHA-256 digest.
 * @return EC_SUCCESS or EC_ERROR_BUSY or EC_ERROR_INVAL
 */
int sha256_init(struct sha256_ctx *pctx)
{
	if (NULL == pctx) {
		return EC_ERROR_INVAL;
	}

	if (MEC17XX_SHA_CTRL & MEC17XX_SHA_CTRL_START) {
		return EC_ERROR_BUSY;
	}

	MEC17XX_SHA_CTRL = 0;
	MEC17XX_SHA_MODE = MEC17XX_SHA256_MODE;
	MEC17XX_SHA_INIT_ADDR = (uint32_t)&pctx->digest.w[0];
	MEC17XX_SHA_RESULT_ADDR = (uint32_t)&pctx->digest.w[0];
	MEC17XX_SHA_RESULT_ADDR = 0;

	MEC17XX_EC_AES_SHA_SWAP_CTRL = 0x03;

	MEC17XX_INT_DISABLE(MEC17XX_SHA_GIRQ) = MEC17XX_SHA_GIRQ_BIT;
	MEC17XX_INT_SOURCE(MEC17XX_SHA_GIRQ) = MEC17XX_SHA_GIRQ_BIT;
#if 0
	task_disable_irq(MEC17XX_IRQ_HASH);
	task_clear_pending_irq(MEC17XX_IRQ_HASH);
#endif
	crypto_memset((void *)pctx, 0, sizeof(struct sha256_ctx));
	crypto_memcpy((void *)pctx->digest.w,
		      (const void *)sha256_init_byte_rev,
		      SHA256_DIGEST_BYTELEN);

	return EC_SUCCESS;
}
/*--------------------------------------------------------------------------*/

/**
 * sha256_update
 * @param pointer data, must be >= 4-byte aligned.
 * @param data length in units of 64 bytes.
 * @param flags bit[0] = 1 enable interrupt before start
 * @return EC_SUCCESS or EC_ERROR_BUSY or EC_ERROR_INVAL
 */
#if 0
int sha256_update(uint32_t *pdata, uint32_t nblocks, uint32_t flags)
{
	int ret;

	if (NULL == pdata) {
		return EC_ERROR_INVAL;
	}

	if (0 != ((uint32_t)pdata & 0x03ul)) {
		return EC_ERROR_INVAL;
	}

	if (MEC17XX_SHA_CTRL & MEC17XX_SHA_CTRL_START) {
		return EC_ERROR_BUSY;
	}

	if (nblocks) {
		MEC17XX_SHA_DATA_ADDR = (uint32_t)pdata;
		MEC17XX_SHA_NBLOCK = nblocks - 1;

		sha_start(0);

		ret = sha_wait2(10 * MSEC);
		if (EC_SUCCESS != ret) {
			return ret;
		}
	}

	return EC_SUCCESS;
}
#else
/*
 * NOTE: SHA HW accelerator computation time is
 * 64 AHB clock cycles per 64 byte chunk.
 * Does not take into account time to fetch 64-byte chunk
 * across AHB fabric.
 */
int sha256_update(struct sha256_ctx *pctx,
		  const void *data,
		  uint32_t nbytes)
{
	uint32_t nblocks, nc;
	int ret;

	if (NULL == pctx) {
		return EC_ERROR_INVAL;
	}

	if (MEC17XX_SHA_CTRL & MEC17XX_SHA_CTRL_START) {
		return EC_ERROR_BUSY;
	}

	nblocks = nc = 0;

	if (NULL == data) {
		return EC_SUCCESS;
	}

	while (nbytes) {
		if ((0 == pctx->block_len) &&
			(0 == ((uint32_t)data & 0x03ul)) &&
			(nbytes >= SHA256_BLOCK_BYTELEN))
		{

			nblocks = (nbytes >> 6);

			MEC17XX_SHA_NBLOCK = nblocks - 1;
			MEC17XX_SHA_DATA_ADDR = (uint32_t)data;

			sha_start(0);

			ret = sha_wait2(10 * MSEC);
			if (EC_SUCCESS != ret) {
				return ret;
			}

			nc = (nblocks << 6);
			pctx->msg_byte_len += nc;
		}
		else
		{
			nc = sha256_copy2block(pctx, data, nbytes);

			if (SHA256_BLOCK_BYTELEN == pctx->block_len) {

				MEC17XX_SHA_NBLOCK = 0; /* one block */
				MEC17XX_SHA_DATA_ADDR = (uint32_t)&(pctx->block.w[0]);

				sha_start(0);

				ret = sha_wait2(1 * MSEC);
				if (EC_SUCCESS != ret) {
					return ret;
				}

				pctx->block_len = 0;
				pctx->msg_byte_len += (SHA256_BLOCK_BYTELEN);
			}
		}

		nbytes -= nc;
		data += nc;

	} // end while (nbytes)

	return EC_SUCCESS;
}
#endif
/*--------------------------------------------------------------------------*/

/**
 * sha256_finalize -
 * @param pointer to 64-byte aligned >= 4-byte buffer for FIPS padding.
 * @param total message length in bytes.
 * @param pointer to optional remaining bytes if message length was not
 * a multiple of 64 bytes.
 * @param number of remaining bytes.
 * @return EC_SUCCESS or EC_ERROR_BUSY or EC_ERROR_INVAL
 */
#if 0
int sha256_finalize(uint32_t *pblock, uint32_t msg_len, uint32_t nrem, uint8_t *prem)
{
	int ret;

	if (NULL == pblock) {
		return EC_ERROR_INVAL;
	}

	if (0 != ((uint32_t)pblock & 0x03ul)) {
		return EC_ERROR_INVAL;
	}

	if (0 != nrem) {
		if (nrem > 64) {
			return EC_ERROR_INVAL;
		}
		if (NULL == prem) {
			return EC_ERROR_INVAL;
		}
	}

	if (MEC17XX_SHA_CTRL & MEC17XX_SHA_CTRL_START) {
		return EC_ERROR_BUSY;
	}

	crypto_memset(pblock, 0, 64);
	crypto_memcpy(pblock, prem, nrem);

	if (nrem < 56) {
		((uint8_t *)pblock)[nrem] = 0x80;
		sha256_msg_bit_len(pblock, msg_len);
		sha256_update(pblock, 1, 0);
		ret = sha_wait();
		if (EC_SUCCESS != ret) {
			MEC17XX_EC_CRYPTO_SRESET = MEC17XX_CRYPTO_AES_SHA_SRST;
			return ret;
		}
	} else {
		if (nrem < 64) {
			((uint8_t *)pblock)[nrem] = 0x80;
		}
		sha256_update(pblock, 1, 0);
		ret = sha_wait();
		if (EC_SUCCESS != ret) {
			MEC17XX_EC_CRYPTO_SRESET = MEC17XX_CRYPTO_AES_SHA_SRST;
			return ret;
		}
		crypto_memset(pblock, 0, 64);
		if (64 == nrem) {
			((uint8_t *)pblock)[0] = 0x80;
		}
		sha256_msg_bit_len(pblock, msg_len);
		sha256_update(pblock, 1, 0);
		ret = sha_wait();
		if (EC_SUCCESS != ret) {
			MEC17XX_EC_CRYPTO_SRESET = MEC17XX_CRYPTO_AES_SHA_SRST;
			return ret;
		}
	}

	return EC_SUCCESS;
}
#else
int sha256_finalize(struct sha256_ctx *pctx)
{
	int ret;

	if (NULL == pctx) {
		return EC_ERROR_INVAL;
	}

	if (MEC17XX_SHA_CTRL & MEC17XX_SHA_CTRL_START) {
		return EC_ERROR_BUSY;
	}

	crypto_memset(&pctx->block.b[pctx->block_len], 0,
			SHA256_BLOCK_BYTELEN - pctx->block_len);

	pctx->block.b[pctx->block_len] = 0x80;

	if (pctx->block_len < 56) {
		pctx->msg_byte_len += pctx->block_len;
		sha256_msg_bit_len(&pctx->block.w[0], pctx->msg_byte_len);
	} else {
		MEC17XX_SHA_NBLOCK = 0; /* one block */
		MEC17XX_SHA_DATA_ADDR = (uint32_t)&(pctx->block.w[0]);

		sha_start(0);

		ret = sha_wait2(1 * MSEC);
		if (EC_SUCCESS != ret) {
			return ret;
		}

		pctx->msg_byte_len += pctx->block_len;

		crypto_memset(&pctx->block.w[0], 0, SHA256_BLOCK_BYTELEN);

		pctx->block_len = 0;

		sha256_msg_bit_len(&pctx->block.w[0], pctx->msg_byte_len);
	}

	MEC17XX_SHA_NBLOCK = 0; /* one block */
	MEC17XX_SHA_DATA_ADDR = (uint32_t)&(pctx->block.w[0]);

	sha_start(0);

	ret = sha_wait2(1 * MSEC);

	return ret;
}
#endif
/*--------------------------------------------------------------------------*/

/* end sha256.c */
/**   @}
 */

