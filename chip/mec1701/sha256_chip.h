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
/** @file sha256_chip.h
 *MEC17xx Device SHA hardware accelerator
 */
/** @defgroup MEC17xx crypto
 */

#ifndef _SHA256_CHIP_H
#define _SHA256_CHIP_H

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


typedef struct sha12_ctx SHA12_CTX;
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


#define MEC17XX_ROM_API_AES_SHA_RST_ADDR	0x00006fb4
extern void aes_sha_reset(void);

#define MEC17XX_ROM_API_AES_SHA_PWR_ADDR	0x00006f90
extern void aes_sha_power(uint8_t pwr_on);

#define MEC17XX_ROM_API_SHA_IS_DONE_ADDR	0x0000881c
extern uint8_t sha_is_done(uint32_t *hwstatus);

#define MEC17XX_ROM_API_SHA_START_ADDR		0x000087b8
extern void sha_start(uint8_t ien);

#define MEC17XX_ROM_API_SHA_INIT_ADDR		0x00008db4
extern uint8_t sha_init(uint8_t mode, uint32_t *pdigest);

#define MEC17XX_ROM_API_SHA_UPDATE_ADDR		0x00008e70
extern uint8_t sha_update(const uint32_t *pdata, uint16_t nblocks,
				uint8_t flags);

#define MEC17XX_ROM_API_SHA_FINAL_ADDR		0x00008ef4
extern uint8_t sha_final(void *padbuf, uint32_t total_msg_len,
				uint8_t *prem, uint8_t flags);


#define MEC17XX_ROM_API_SHA12_INIT_ADDR		0x00008860
extern uint8_t sha12_init(SHA12_CTX *pctx, uint8_t sha_mode);

#define MEC17XX_ROM_API_SHA12_UPDATE_ADDR	0x0000891c
extern uint8_t sha12_update(SHA12_CTX *pctx, const uint32_t *pdata, 
				uint32_t data_len);

#define MEC17XX_ROM_API_SHA12_FINALIZE_ADDR	0x000089dc
extern uint8_t sha12_finalize(SHA12_CTX *pctx);


#ifdef __cplusplus
}
#endif

#endif // #ifndef _SHA256_CHIP_H
/**   @}
 */

