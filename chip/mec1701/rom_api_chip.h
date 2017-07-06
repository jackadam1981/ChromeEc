/* Copyright 2017 The Chromium OS Authors. All rights reserved
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Register map for MEC17xx processor
 */
/** @file rom_apis.h
 *MEC17xx ROM API's
 */
/** @defgroup MEC17xx ROM API
 */

#ifndef _ROM_API_CHIP_H
#define _ROM_API_CHIP_H

#include <stdint.h>
#include <stddef.h>


#define SHA256_DIGEST_BYTELEN	(32ul)
#define SHA256_DIGEST_WORDLEN	(8ul)

/*
 * SHA-1 and SHA-256 use the same block length = 64 bytes
 * Both also use the same message bit length size of 8 bytes.
 */

#define SHA256_BLOCK_BYTELEN	(64ul)
#define SHA256_BLOCK_WORDLEN	(16ul)


/* Maximum SHA-1 and SHA-256 byte length */
#define SHA256_MSG_LEN_MAX  (0x1FFFFFFFFFFFFFFFULL)


struct sha12_ctx {
	union {
		uint32_t w[SHA256_DIGEST_WORDLEN];
		uint8_t  b[SHA256_DIGEST_BYTELEN];
	} digest;
	union {
		uint32_t w[SHA256_BLOCK_WORDLEN];
		uint8_t  b[SHA256_BLOCK_BYTELEN];
	} block;
	uint8_t  mode;
	uint8_t  block_len;
	uint8_t  rsvd[2];
	uint32_t total_msg_len;
};


#ifdef __cplusplus
extern "C" {
#endif

/* Place any C interfaces here */

/* MEC17xx ROM API */

#define MEC17XX_ROM_SHA_RET_OK			0
#define MEC17XX_ROM_SHA_RET_START		1
#define MEC17XX_ROM_SHA_RET_ERROR		0x80
#define MEC17XX_ROM_SHA_RET_ERR_BUSY		0x80
#define MEC17XX_ROM_SHA_RET_ERR_BAD_ADDR	0x81
#define MEC17XX_ROM_SHA_RET_ERR_TIMEOUT		0x82
#define MEC17XX_ROM_SHA_RET_ERR_MAX_LEN		0x83
#define MEC17XX_ROM_SHA_RET_ERR_UNSUPPORTED	0x84

#define MEC17XX_ROM_SHA_MODE_1		1
#define MEC17XX_ROM_SHA_MODE_256	3
#define MEC17XX_ROM_SHA_MODE_512	5

#define MEC17XX_ROM_SHA_FLAG_CLR_STS	0x01
#define MEC17XX_ROM_SHA_FLAG_IEN	0x02
#define MEC17XX_ROM_SHA_FLAG_START	0x04

/* NOTE bit[0]=1 indicating Cortex-M4 Thumb2 mode */


#define QMSPI_SPI_MODE0	(0)
#define QMSPI_SPI_MODE3	(0x07)

#define QMSPI_IFCTRL_DFLT	(0x05)

#define QMSPI_FREQ_48M	(48000000ul)
#define QMSPI_FREQ_24M	(24000000ul)
#define QMSPI_FREQ_16M	(16000000ul)
#define QMSPI_FREQ_12M	(12000000ul)

extern void rom_qmspi_init(uint32_t freq_hz, uint8_t spi_sig,
			   uint8_t ifctrl);

/*
 * if status pointer is not NULL fills in
 * *status with copy of QMSPI.Status register.
 * Returns 1 if QMSPI is done else 0.
 */
extern uint8_t rom_qmspi_is_done(uint32_t *status);

/*
 * Set DMA channel's Run bit.
 * Clears QMSPI.Status
 * Enables QMSPI interrupt sources specified by ien_mask
 * Start QMSPI transaction.
 * Responsibility of caller to configure ECIA & NVIC.
 */
extern void rom_qmspi_start_dma(uint8_t dma_chan, uint16_t ien_mask);

/*
 * spi_cmd - encoding of SPI read command
 *	b[7:0]= SPI read op-code
 *	number of pins to transmit cmd, address, and data
 *	00b = 1 pin, 01b = 2 pins, 10b = 4 pins
 *	b[9:8]= cmd num pins
 *	b[11:10]= address num pins
 *	b[13:12]= data num pins
 *	b[15]= 0(no mode byte), 1(xmit mode byte after address)
 *	b[23:16]= mode byte
 *	b[31:24]= number of dummy bytes = dummy clocks * clocks/byte
 *		  where clocks/byte = 8 for single,
 *		  4 for dual, or 2 for quad.
 *
 * spi_address = 24-bit SPI flash address
 * mem_addr = address of SRAM buffer for data
 * nbytes = requested number of bytes to read
 * dma_chan = zero based DMA channel ID to use.
 *
 * return number of bytes qmspi configured to read. May be less
 * than requested (nbytes).
 */

#define QMSPI_READ_111		(0x00000003ul)
#define QMSPI_READ_111_FAST	(0x0100000Bul)
#define QMSPI_READ_112_FAST	(0x0200103Bul)
#define QMSPI_READ_114_FAST	(0x0400206Bul)

extern uint32_t rom_qmspi_cfg_read_dma(uint32_t spi_cmd,
				       uint32_t spi_address,
				       uint32_t mem_addr,
				       uint32_t nbytes,
				       uint8_t dma_chan);

#define MEC17XX_ROM_API_AES_SHA_RST_ADDR	0x00006fb4
extern void rom_aes_sha_reset(void);

#define MEC17XX_ROM_API_AES_SHA_PWR_ADDR	0x00006f90
extern void rom_aes_sha_power(uint8_t pwr_on);

#define MEC17XX_ROM_API_HASH_BUSY_ADDR		0x000087dc
extern uint8_t rom_hash_busy(void);

extern uint8_t rom_hash_status(void);

extern void rom_hash_iclr(void);

#define MEC17XX_ROM_API_SHA_IS_DONE_ADDR	0x0000881c
extern uint8_t rom_sha_is_done(uint32_t *hwstatus);

#define MEC17XX_ROM_API_SHA_START_ADDR		0x000087b8
extern void rom_sha_start(uint8_t ien);

#define MEC17XX_ROM_API_SHA_INIT_ADDR		0x00008db4
extern uint8_t rom_sha_init(uint8_t mode, uint32_t *pdigest);

#define MEC17XX_ROM_API_SHA_UPDATE_ADDR		0x00008e70
extern uint8_t rom_sha_update(const uint32_t *pdata, uint16_t nblocks,
				uint8_t flags);

#define MEC17XX_ROM_API_SHA_FINAL_ADDR		0x00008ef4
extern uint8_t rom_sha_final(void *padbuf, uint32_t total_msg_len,
				const uint8_t *prem, uint8_t flags);

#define MEC17XX_ROM_API_SHA12_INIT_ADDR		0x00008860
extern uint8_t rom_sha12_init(struct sha12_ctx *pctx, uint8_t sha_mode);

#define MEC17XX_ROM_API_SHA12_UPDATE_ADDR	0x0000891c
extern uint8_t rom_sha12_update(struct sha12_ctx *pctx, const uint32_t *pdata,
				uint32_t data_len);

#define MEC17XX_ROM_API_SHA12_FINALIZE_ADDR	0x000089dc
extern uint8_t rom_sha12_finalize(struct sha12_ctx *pctx);

/*
 * Copy SHA-256 initial value from FIPS specification
 * into the >= 4 byte aligned 32-byte buffer pointed to
 * by digest. Initial value is stored in LSB first byte
 * ordering required by Hash engine.
 * This routine does not touch hardware.
 */
#define MEC17XX_ROM_API_SHA256_RAW_INIT_ADDR	0x00008b38
extern void rom_sha256_raw_init(uint32_t *digest);

/*
 * Start Hash engine for SHA-256 computation of nblocks 64-byte
 * blocks of data.
 * data points to >= 4-byte aligned data whose length is a multiple
 * of 64 bytes.
 * data point to >= 4-byte aligned 32-byte buffer containing the
 * previous digest value and updated when calculation done.
 * Routine returns after starting Hash engine. Caller should use
 * rom_hash_busy() to poll for done.
 * Caller may also enable Hash engine interrupt in ECIA & NVIC
 * before calling this routine. Hash Done ISR can signal when
 * operation is finished.
 * return values are
 * 0 = success, Hash engine started if nblocks != 0
 * 0x80 = Hash engine is busy
 * 0x81 = data and/or digest are NULL
 */
#define MEC17XX_ROM_API_SHA256_RAW_UPDATE_ADDR	0x00008b5c
extern uint8_t rom_sha256_raw_update(const uint32_t *data, uint32_t *digest,
				     uint32_t nblocks);

/*
 * NDRNG
 */
#define MEC17XX_ROM_API_RNG_POWER_ADDR	0x000073f0
extern void rom_rng_power(uint8_t pwr_on);

#define MEC17XX_ROM_API_RNG_RESET_ADDR	0x00007388
extern void rom_rng_reset(void);

#define MEC17XX_ROM_API_RNG_MODE_ADDR	0x00007425
#define MEC17XX_RNG_MODE_NON_DETERMINISTIC	0
#define MEC17XX_RNG_MODE_PSEUDO_RAND		1
extern void rom_rng_mode(uint8_t pseudo_rand_mode);

#define MEC17XX_ROM_API_RNG_IS_ON_ADDR	0x00007404
extern uint8_t rom_rng_is_on(void);

#define MEC17XX_ROM_API_RNG_START_ADDR	0x000073C4
extern void rom_rng_start(void);

#define MEC17XX_ROM_API_RNG_STOP_ADDR	0x000073D4
extern void rom_rng_stop(void);

#define MEC17XX_ROM_API_RNG_FIFO_LVL_ADDR	0x000073E4
extern uint32_t rom_rng_get_fifo_level(void);

#define MEC17XX_ROM_API_RNG_GET_BYTES_ADDR	0x00007440
extern uint32_t rom_rng_get_bytes(uint8_t *dest, uint32_t nbytes);

#define MEC17XX_ROM_API_RNG_GET_WORDS_ADDR	0x00007488
extern uint32_t rom_rng_get_words(uint32_t *dest, uint32_t nwords);


#ifdef __cplusplus
}
#endif

#endif /* #ifndef _ROM_API_CHIP_H */
/**   @}
 */

