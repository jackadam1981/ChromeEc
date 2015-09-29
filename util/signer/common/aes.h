#ifndef _F_AES_H__
#define _F_AES_H__

#include <stddef.h>
#include <inttypes.h>

class AES {
 private:
  unsigned char key_[16];
 public:
  AES();
  ~AES();

  void set_key(const void* key);
  void decrypt_block(const void* in, void* out);
  void encrypt_block(const void* in, void* out);
  void cmac(const void* in, size_t in_len, void* out);
};

#endif  // _F_AES_H__
