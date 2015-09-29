#ifndef _F_ECDH_H__
#define _F_ECDH_H__

#include <openssl/ec.h>

class ECDH {
 private:
  EC_KEY* key_;
  EC_GROUP* group_;
 public:
  ECDH();
  ~ECDH();

  void get_point(void* dst);

  // Computes SHA256 of x-coordinate.
  void compute_secret(const void* other, void* secret);
};

#endif  // _F_ECDH_H__
