#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <common/publickey.h>

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

#include "intelhex.h"
#include "signed_header.h"

IntelHex::IntelHex(const char* filename, int expected_low)
    : success(true), low(4*65536), high(0), seg(0) {
  memset(mem, 0xff, sizeof(mem));  // default memory content
  bool isRam = false;

  FILE* fp = fopen(filename, "rt");
  if (fp != NULL) {
    char line[BUFSIZ];
    while (fgets(line, sizeof(line), fp)) {
      if (strchr(line, '\r')) *strchr(line, '\r') = 0;
      if (strchr(line, '\n')) *strchr(line, '\n') = 0;
      if (line[0] != ':') continue;  // assume comment line
      if (strlen(line) < 9) {
        fprintf(stderr, "short record %s", line);
        success = false;
        continue;
      }
      if (line[7] != '0') {
          fprintf(stderr, "unknown record type %s", line);
          success = false;
      } else switch (line[8]) {
        case '1': {  // 01 eof
        } break;
        case '2': {  // 02 segment
          if (!strncmp(line, ":02000002", 9)) {
            char* p = line + 9;
            int s = parseWord(&p);
            if (s != 0x1000) {
              if (s >= 0x4000 && s <= 0xb000) {
                seg = s - 0x4000;
                // fprintf(stderr, "at segment %04x\n", seg);
              } else {
                fprintf(stderr, "segment should be 0x4000..0xb000 or 0x1000: %s\n", line);
                success = false;
              }
            }
          }
          isRam = !strcmp(line, ":020000021000EC");
        } break;
        case '0': {  // 00 data
          char* p = line + 1;
          int len = parseByte(&p);
          int adr = parseWord(&p);
          parseByte(&p);
          while (len--) {
            if (isRam) {
              int v = parseByte(&p);
              if (v != 0) {
                fprintf(stderr, "WARNING: non-zero RAM byte %02x at %04x\n",
                        v, adr);

              }
              ++adr;
            } else {
              store((seg<<4) + adr++, parseByte(&p));
            }
          }
        } break;
        case '3': {  // 03 entry point
        } break;
        default: {
          fprintf(stderr, "unknown record type %s", line);
          success = false;
        } break;
      }
    }
    fclose(fp);
  } else {
    fprintf(stderr, "failed to open file '%s'\n", filename);
    success = false;
  }

  if (success && low != expected_low) {
    fprintf(stderr, "low should be 0x%04x, is 0x%04x\n", expected_low, low);
    success = false;
  }

  if (success) {
    fprintf(stderr, "low %x, high %x\n", low, high);
    high = ((high + 2047) / 2048) * 2048;
    fprintf(stderr, "low %x, rhigh %x\n", low, high);
  }
}

IntelHex::~IntelHex() {}

size_t IntelHex::size() { return high; }

void IntelHex::print() {
  for (int i = 0; i < high; i += 16) {
    // spit out segment record at start of segment.
    if (!(i&0xffff)) {
      int s = 0x4000 + (i>>4);
      printf(":02000002%04X%02X\n", s,
             (~((2 + 2 + (s>>8)) & 255) + 1) & 255);
    }
    // spit out data records, 16 bytes each.
    printf(":10%04x00", i&0xffff);
    int crc = 16 + ((i>>8)&255) + (i&255);
    for (int n = 0; n < 16; ++n) {
      printf("%02x", mem[i+n]);
      crc += mem[i+n];
    }
    printf("%02x", (~(crc & 255) + 1) & 255);
    printf("\n");
  }
}

int IntelHex::nibble(char n) {
  switch (n) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return n - '0';
    case 'a': case 'A': return 10;
    case 'b': case 'B': return 11;
    case 'c': case 'C': return 12;
    case 'd': case 'D': return 13;
    case 'e': case 'E': return 14;
    case 'f': case 'F': return 15;
    default:
      fprintf(stderr, "bad hex digit '%c'\n", n);
      success = false;
      return 0;
  }
}

int IntelHex::parseByte(char** p) {
  int result = nibble(**p);
  result *= 16;
  (*p)++;
  result |= nibble(**p);
  (*p)++;
  return result;
}

int IntelHex::parseWord(char** p) {
  int result = parseByte(p);
  result *= 256;
  result |= parseByte(p);
  return result;
}

void IntelHex::store(int adr, int v) {
  if (adr < 0 || (size_t)(adr) >= sizeof(mem)) {
    fprintf(stderr, "illegal adr %04x\n", adr);
    success = false;
    return;
  }
  mem[adr] = v;
  if (adr > high) high = adr;
  if (adr < low) low = adr;
}

void IntelHex::sign(PublicKey& key, const SignedHeader* input_hdr) {
  BIGNUM* sig = NULL;
  SignedHeader* hdr = (SignedHeader*)(&mem[0]);
  int result;

  memcpy(hdr, input_hdr, sizeof(SignedHeader));

  result = key.sign(&hdr->tag, high - offsetof(SignedHeader, tag), &sig);

  // Compute hash by hand as well.
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_CTX sha256;
  SHA256_Init(&sha256);
  SHA256_Update(&sha256, &hdr->tag, high - offsetof(SignedHeader, tag));
  SHA256_Final(hash, &sha256);
  fprintf(stderr, "hash:");
  for (size_t i = 0; i < sizeof(hash); ++i) {
    fprintf(stderr, "%02x", hash[i]);
  }
  fprintf(stderr, "\n");

  if (result != 1) {
    fprintf(stderr, "ossl_sign:%d\n", result);
  } else {
    hdr->image_size = high;

    size_t nwords = key.nwords();
    key.toArray(hdr->signature, nwords, sig);
  }

  if (sig) BN_free(sig);
}
