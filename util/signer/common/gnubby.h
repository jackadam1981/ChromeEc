#ifndef _F_GNUBBY_H__
#define _F_GNUBBY_H__

#include <stddef.h>
#include <inttypes.h>

#include <libusb-1.0/libusb.h>

typedef struct env_md_ctx_st EVP_MD_CTX;
typedef struct evp_pkey_st EVP_PKEY;

class Gnubby {
 public:
  Gnubby();
  ~Gnubby();

  bool ok() const { return handle_ != NULL; }

  int Sign(EVP_MD_CTX* ctx, uint8_t* signature, uint32_t* siglen, EVP_PKEY* key);

 private:
  int send_to_device(uint8_t instruction,
                     const uint8_t* payload,
                     size_t length);

  int receive_from_device(uint8_t* dest, size_t length);

  libusb_context* ctx_;
  libusb_device_handle* handle_;
};

#define MAX_APDU_SIZE 1200
#define LIBUSB_ERR -1

#define VERBOSE

#endif  // _F_GNUBBY_H__
