#include "boot_loader.h"

STATIC
void _sha_write(const void* data, size_t n) {
  const uint8_t* bp = (const uint8_t*)data;
  const uint32_t* wp;

  while (n != 0 && ((uint32_t)bp & 3)) {  // Feed unaligned start bytes.
    write_reg8(KEYMGR0_BASE_ADDR+KEYMGR_SHA_INPUT_FIFO_OFFSET, *bp++);
    n -= 1;
  }

  wp = (uint32_t*)bp;
  while (n >= 8*4) { // Feed groups of aligned words.
    write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_INPUT_FIFO_OFFSET, *wp++);
    write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_INPUT_FIFO_OFFSET, *wp++);
    write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_INPUT_FIFO_OFFSET, *wp++);
    write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_INPUT_FIFO_OFFSET, *wp++);
    write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_INPUT_FIFO_OFFSET, *wp++);
    write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_INPUT_FIFO_OFFSET, *wp++);
    write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_INPUT_FIFO_OFFSET, *wp++);
    write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_INPUT_FIFO_OFFSET, *wp++);
    n -= 8*4;
  }
  while (n >= 4) {  // Feed individual aligned words.
    write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_INPUT_FIFO_OFFSET, *wp++);
    n -= 4;
  }

  bp = (uint8_t*)wp;
  while (n != 0) {  // Feed remaing bytes.
    write_reg8(KEYMGR0_BASE_ADDR+KEYMGR_SHA_INPUT_FIFO_OFFSET, *bp++);
    n -= 1;
  }
}

STATIC
void _sha_wait(uint32_t* digest) {
  uint32_t itop;

  // Wait for result.
  // TODO: what harm does glitching do? Read out non-digest? Old digest?
  do {
    itop = read_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_ITOP_OFFSET);
  } while (!itop);

  // Read out final digest.
  for (int i = 0; i < 8; ++i) {
    *digest++ = read_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_STS_H0_OFFSET + i * 4);
  }

  write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_ITOP_OFFSET, 0);  // clear status
}

void hwSHA256(const void* data, size_t n, uint32_t* digest) {
  write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_ITOP_OFFSET, 0);  // clear status

  write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_CFG_MSGLEN_LO_OFFSET, n);
  write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_CFG_MSGLEN_HI_OFFSET, 0);

  write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_CFG_EN_OFFSET, KEYMGR_SHA_CFG_EN_INT_EN_DONE_MASK);
  write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_TRIG_OFFSET, KEYMGR_SHA_TRIG_TRIG_GO_MASK);

  _sha_write(data, n);
  _sha_wait(digest);
}

void hwSHA256_init(hwSHA256_CTX* ctx) {
  write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_ITOP_OFFSET, 0);  // clear status

  write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_CFG_EN_OFFSET,
            KEYMGR_SHA_CFG_EN_LIVESTREAM_MASK | KEYMGR_SHA_CFG_EN_INT_EN_DONE_MASK);

  write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_TRIG_OFFSET, KEYMGR_SHA_TRIG_TRIG_GO_MASK);
}

void hwSHA256_update(hwSHA256_CTX* ctx, const void* data, size_t n) {
  _sha_write(data, n);
}

const uint8_t* hwSHA256_final(hwSHA256_CTX* ctx) {
  write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_TRIG_OFFSET, KEYMGR_SHA_TRIG_TRIG_STOP_MASK);

  _sha_wait(ctx->digest);

  return (const uint8_t*)(ctx->digest);
}

void hwKeyLadderStep(uint32_t cert, const uint32_t* input) {
  write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_ITOP_OFFSET, 0);  // clear status

  VERBOSE("Cert %2u: ", cert);

  write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_USE_CERT_INDEX_OFFSET,
            (cert << KEYMGR_SHA_USE_CERT_INDEX_LSB) |
            KEYMGR_SHA_USE_CERT_ENABLE_MASK);

  write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_CFG_EN_OFFSET, KEYMGR_SHA_CFG_EN_INT_EN_DONE_MASK);
  write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_TRIG_OFFSET, KEYMGR_SHA_TRIG_TRIG_GO_MASK);

  if (input != NULL) {
    for (int i = 0; i < 8; ++i) {
      write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_INPUT_FIFO_OFFSET, *input++);
    }
    write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_TRIG_OFFSET, KEYMGR_SHA_TRIG_TRIG_STOP_MASK);
  }

  uint32_t itop;
  do {
    itop = read_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_ITOP_OFFSET);
  } while (!itop);

  write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_ITOP_OFFSET, 0);  // clear status

  uint32_t flags = read_reg(KEYMGR0_BASE_ADDR+KEYMGR_HKEY_ERR_FLAGS_OFFSET);
  if (flags) {
    INFO("Cert %2u: fail %x\n", cert, flags);
  } else {
    VERBOSE("flags %x\n", flags);
  }
}
