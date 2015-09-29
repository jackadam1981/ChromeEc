#ifndef _F_IMAGE_H__
#define _F_IMAGE_H__

#include <stddef.h>
#include <inttypes.h>

#include <string>

class PublicKey;
struct SignedHeader;

class Image {
 public:
  Image();
  ~Image();

  bool fromIntelHex(const std::string& filename, bool withSignature = true);
  bool fromElf(const std::string& filename);

  bool sign(PublicKey& key, const SignedHeader* hdr, const uint32_t fuses[]);

  void toIntelHex(const std::string& filename) const;

  bool ok() const { return success_; }
  const uint8_t* code() const { return mem_; }
  size_t size() const { return high_ - base_; }
  int base() const { return base_; }
  int ro_base() const { return ro_base_; }
  int rx_base() const { return rx_base_; }
  int ro_max() const { return ro_max_; }
  int rx_max() const { return rx_max_; }

 private:
  int nibble(char n);
  int parseByte(char** p);
  int parseWord(char** p);
  void store(int adr, int v);

  bool success_;
  uint8_t mem_[8*64*1024];
  int low_, high_, base_;
  size_t ro_base_, rx_base_;
  size_t ro_max_, rx_max_;
};

#endif  // _F_IMAGE_H__
