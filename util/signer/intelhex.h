#ifndef _F_INTELHEX_H__
#define _F_INTELHEX_H__

#include <stddef.h>
#include <inttypes.h>

class PublicKey;
struct SignedHeader;

class IntelHex {
 public:
  IntelHex(const char* filename, int expected_low = 0);
  ~IntelHex();

  bool ok() { return success; }
  void print();
  const uint8_t* code() { return mem; }
  size_t size();
  void sign(PublicKey& key, const SignedHeader* hdr);

 private:
  int nibble(char n);
  int parseByte(char** p);
  int parseWord(char** p);
  void store(int adr, int v);

  bool success;
  uint8_t mem[8*64*1024];
  int low, high, seg;
};

#endif  // _F_INTELHEX_H__
