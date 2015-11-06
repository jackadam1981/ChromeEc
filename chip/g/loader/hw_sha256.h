#ifndef _F_HW_SHA256_H__
#define _F_HW_SHA256_H__

#include <inttypes.h>
#include <stddef.h>

#define SHA256_DIGEST_BYTES 32
#define SHA256_DIGEST_WORDS (32 / sizeof(uint32_t))

typedef struct hwSHA256_CTX {
  uint32_t digest[SHA256_DIGEST_WORDS];
} hwSHA256_CTX;

void hwSHA256_init(hwSHA256_CTX* ctx);
void hwSHA256_update(hwSHA256_CTX* ctx, const void* data, size_t len);
const uint8_t* hwSHA256_final(hwSHA256_CTX* ctx);

void hwSHA256(const void* data, size_t len, uint32_t* digest);

void hwKeyLadderStep(uint32_t certificate, const uint32_t* input);

#endif  // _F_HW_SHA256_H__
