#include "boot_loader.h"

#include "loader-testkey-A.h"
#include "loader-testkey-B.h"

#define RANDOM_UINT16 (cyclecounter()&0xffffu)  // TODO: draw from t(p)rng
#define RANDOM_STEP 5
#define RANDOM_START RANDOM_UINT16  // TODO: draw from t(p)rng

STATIC
void print(const char* tag, const void* data, size_t len) {
  VERBOSE("%s:%h\n", tag, len, data);
}

// Montgomery c[] += a * b[] / R % key.
STATIC
void montMulAdd(const uint32_t* key,
                uint32_t* c, const uint32_t a, const uint32_t* b) {
  uint32_t A, B, d0;

  {
    register uint64_t tmp;
    tmp = c[0] + (uint64_t)a * b[0];
    A = tmp >> 32;
    d0 = (uint32_t)tmp * *key++;
    tmp = (uint32_t)tmp + (uint64_t)d0 * *key++;
    B = tmp >> 32;
  }

  for (int i = 0; i < RSA_NUM_WORDS - 1; ++i) {
    register uint64_t tmp;
    tmp = A + (uint64_t)a * b[i + 1] + c[i + 1];
    A = tmp >> 32;
    tmp = B + (uint64_t)d0 * *key++ + (uint32_t)tmp;
    c[i] = (uint32_t)tmp;
    B = tmp >> 32;
  }

  c[RSA_NUM_WORDS - 1] = A + B;
}

// Montgomery c[] = a[] * b[] / R % key.
STATIC
void montMul(const uint32_t* key,
             uint32_t* c, const uint32_t* a, const uint32_t* b) {
  for (int i = 0; i < RSA_NUM_WORDS; ++i) {
    c[i] = 0;
  }
  for (int i = 0; i < RSA_NUM_WORDS; ++i) {
    montMulAdd(key, c, a[i], b);
  }
}

// Montgomery c[] = a[] * 1 / R % key.
STATIC
void montMul1(const uint32_t* key,
              uint32_t* c, const uint32_t* a) {
  for (int i = 0; i < RSA_NUM_WORDS; ++i) {
    c[i] = 0;
  }
  montMulAdd(key, c, 1, a);
  for (int i = 1; i < RSA_NUM_WORDS; ++i) {
    montMulAdd(key, c, 0, a);
  }
}

#if LOADERKEYEXP == 65537
// In-place exponentiation to power 65537 % key.
STATIC
void modpow(const uint32_t* key,
            const uint32_t* signature, uint8_t* out) {
  static uint32_t aR[RSA_NUM_WORDS];
  static uint32_t aaR[RSA_NUM_WORDS];
#define aaa aaR  // alias

  for (int i = 0; i < RSA_NUM_WORDS; ++i) {
    aR[i] = signature[i];
  }
  for (int i = 0; i < 16; i += 2) {
    montMul(key, aaR, aR, aR);  // aaR = aR * aR / R mod M
    montMul(key, aR, aaR, aaR);  // aR = aaR * aaR / R mod M
  }
  montMul(key, aaa, aR, signature);  // aaa = aR * a / R mod M
  montMul1(key, out, aaa);  // convert from Montgomery
#undef aaa
}
#endif

#if LOADERKEYEXP == 3
// In-place exponentiation to power 3 % key.
STATIC
void modpow(const uint32_t* key,
            const uint32_t* signature, uint32_t* out) {
  static uint32_t aaR[RSA_NUM_WORDS];
  static uint32_t aaaR[RSA_NUM_WORDS];

  montMul(key, aaR, signature, signature);
  montMul(key, aaaR, aaR, signature);
  montMul1(key, out, aaaR);
}
#endif

void LOADERKEY_verify(uint32_t keyid, const uint32_t* signature, const uint32_t* sha256) {
  static uint32_t buf[RSA_NUM_WORDS] __attribute__((section(".guarded_data")));
  static uint32_t hash[SHA256_DIGEST_WORDS] __attribute__((section(".guarded_data")));
  uint32_t step, offset;

  const uint32_t* key = LOADERKEY_A;

  if (keyid == LOADERKEY_B[0]) {
    key = LOADERKEY_B;
  }

  MEASURE("exp  ", modpow(key, signature, buf));

  print("sig ", buf, RSA_NUM_BYTES);

  // XOR in offsets across buf.
  // Mostly to get rid of all those -1 words in there.
  offset = RANDOM_START % RSA_NUM_WORDS;
  step = (RANDOM_STEP % RSA_NUM_WORDS) || 1;
  for (int i = 0; i < RSA_NUM_WORDS; ++i) {
    buf[offset] ^= (0x1000u + offset);
    offset = (offset + step) % RSA_NUM_WORDS;
  }

  // Xor digest location, so all words becomes 0 only iff equal.
  //
  // Also XOR in offset and non-zero const.
  // This to avoid repeat glitches to zero be able to produce the right result.
  offset = RANDOM_START % SHA256_DIGEST_WORDS;
  step = (RANDOM_STEP % SHA256_DIGEST_WORDS) || 1;
  for (int i = 0; i < SHA256_DIGEST_WORDS; ++i) {
    buf[offset] ^= bswap(sha256[SHA256_DIGEST_WORDS - 1 - offset]) ^ (offset + 0x10u);
    offset = (offset + step) % SHA256_DIGEST_WORDS;
  }

  print("sig^", buf, RSA_NUM_BYTES);

  // Hash resulting buffer.
  hwSHA256(buf, RSA_NUM_BYTES, hash);

  print("hash", hash, SHA256_DIGEST_BYTES);

  // Write computed hash to unlock register to unlock execution, iff right.
  //
  // Idea is that this flow cannot be glitched to have correct values
  // with any probability.
  for (int i = 0; i < SHA256_DIGEST_WORDS; ++i) {
    write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_SB_BL_SIG0_OFFSET+i*4, hash[i]);
  }

  // Make unlock attempt
  // Value written is irrelevant, as long as something is written
  write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_SIG_UNLOCK_OFFSET, 1);
}
